#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#    include <unistd.h>
#endif

#include "./pdf_viewer.h"

static void pdf_viewer_set_status(Pdf_Viewer *pdf, const char *message)
{
    pdf->status.count = 0;
    sb_append_cstr(&pdf->status, message);
    sb_append_null(&pdf->status);
}

static char *shell_quote(const char *s)
{
    size_t len = 2;
    for (const char *p = s; *p; ++p) {
        len += (*p == '\'') ? 4 : 1;
    }

    char *result = malloc(len + 1);
    assert(result != NULL && "Buy more RAM lol");

    char *out = result;
    *out++ = '\'';
    for (const char *p = s; *p; ++p) {
        if (*p == '\'') {
            memcpy(out, "'\\''", 4);
            out += 4;
        } else {
            *out++ = *p;
        }
    }
    *out++ = '\'';
    *out = '\0';
    return result;
}

static bool read_ppm_token(FILE *f, char *buf, size_t buf_size)
{
    int c = fgetc(f);
    while (c != EOF) {
        if (isspace(c)) {
            c = fgetc(f);
        } else if (c == '#') {
            while (c != EOF && c != '\n') c = fgetc(f);
            c = fgetc(f);
        } else {
            break;
        }
    }

    if (c == EOF) return false;

    size_t n = 0;
    while (c != EOF && !isspace(c)) {
        if (n + 1 < buf_size) {
            buf[n++] = (char)c;
        }
        c = fgetc(f);
    }
    buf[n] = '\0';
    return n > 0;
}

static bool load_ppm(const char *file_path, unsigned char **pixels, int *width, int *height)
{
    bool result = true;
    FILE *f = NULL;
    char token[64];

    f = fopen(file_path, "rb");
    if (f == NULL) return false;

    if (!read_ppm_token(f, token, sizeof(token)) || strcmp(token, "P6") != 0) return_defer(false);
    if (!read_ppm_token(f, token, sizeof(token))) return_defer(false);
    *width = atoi(token);
    if (!read_ppm_token(f, token, sizeof(token))) return_defer(false);
    *height = atoi(token);
    if (!read_ppm_token(f, token, sizeof(token)) || atoi(token) != 255) return_defer(false);

    if (*width <= 0 || *height <= 0) return_defer(false);

    size_t pixels_size = (size_t)*width * (size_t)*height * 3;
    *pixels = malloc(pixels_size);
    assert(*pixels != NULL && "Buy more RAM lol");

    if (fread(*pixels, 1, pixels_size, f) != pixels_size) {
        free(*pixels);
        *pixels = NULL;
        return_defer(false);
    }

defer:
    if (f) fclose(f);
    return result;
}

static bool pdf_viewer_render_page(Pdf_Viewer *pdf)
{
#ifdef _WIN32
    pdf_viewer_set_status(pdf, "PDF ghosts are currently POSIX-only. Windows stagehands wanted.");
    return false;
#else
    bool result = true;
    char tmp_template[] = "/tmp/ded-pdf-XXXXXX";
    char *tmp_dir = NULL;
    char ppm_path[4096];
    char prefix_path[4096];
    char command[16384];
    unsigned char *pixels = NULL;

    tmp_dir = mkdtemp(tmp_template);
    if (tmp_dir == NULL) {
        pdf_viewer_set_status(pdf, "Could not make a tiny PDF theatre in /tmp. Tragic.");
        return false;
    }

    snprintf(prefix_path, sizeof(prefix_path), "%s/page", tmp_dir);
    snprintf(ppm_path, sizeof(ppm_path), "%s.ppm", prefix_path);

    char *quoted_file_path = shell_quote(pdf->file_path.items);
    char *quoted_prefix_path = shell_quote(prefix_path);
    snprintf(command, sizeof(command),
             "pdftoppm -f %d -l %d -r 144 -singlefile %s %s > /dev/null 2>&1",
             pdf->page, pdf->page, quoted_file_path, quoted_prefix_path);
    free(quoted_file_path);
    free(quoted_prefix_path);

    int rc = system(command);
    if (rc != 0) {
        if (pdf->page > 1) {
            pdf->page -= 1;
            pdf_viewer_set_status(pdf, "The next page refused its entrance. Curtain stays here.");
        } else {
            pdf_viewer_set_status(pdf, "pdftoppm could not summon this PDF. Install poppler, then cue thunder.");
        }
        return_defer(false);
    }

    if (!load_ppm(ppm_path, &pixels, &pdf->width, &pdf->height)) {
        pdf_viewer_set_status(pdf, "The PDF became pixels, then immediately forgot how. Very on brand.");
        return_defer(false);
    }

    if (pdf->texture == 0) {
        glGenTextures(1, &pdf->texture);
    }

    glBindTexture(GL_TEXTURE_2D, pdf->texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, pdf->width, pdf->height, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);

    pdf->ready = true;
    pdf->dirty = false;
    pdf_viewer_set_status(pdf, "PDF rendered. The document has entered, wearing pixels.");

defer:
    free(pixels);
    remove(ppm_path);
    rmdir(tmp_dir);
    return result;
#endif
}

void pdf_viewer_open(Pdf_Viewer *pdf, const char *file_path)
{
    pdf->file_path.count = 0;
    sb_append_cstr(&pdf->file_path, file_path);
    sb_append_null(&pdf->file_path);

    pdf->page = 1;
    pdf->dirty = true;
    pdf->ready = false;
    pdf_viewer_set_status(pdf, "PDF detected. Applying dramatic lighting...");
}

void pdf_viewer_next_page(Pdf_Viewer *pdf)
{
    pdf->page += 1;
    pdf->dirty = true;
    pdf->ready = false;
    pdf_viewer_set_status(pdf, "Turning the PDF page with unnecessary intensity...");
}

void pdf_viewer_prev_page(Pdf_Viewer *pdf)
{
    if (pdf->page > 1) {
        pdf->page -= 1;
        pdf->dirty = true;
        pdf->ready = false;
        pdf_viewer_set_status(pdf, "Rewinding the PDF drama...");
    }
}

static void pdf_viewer_render_text(Free_Glyph_Atlas *atlas, Simple_Renderer *sr, const char *text, Vec2f pos, Vec4f color)
{
    free_glyph_atlas_render_line_sized(atlas, sr, text, strlen(text), &pos, color);
}

void pdf_viewer_render(Pdf_Viewer *pdf, SDL_Window *window, Free_Glyph_Atlas *atlas, Simple_Renderer *sr)
{
    int w, h;
    SDL_GetWindowSize(window, &w, &h);

    sr->resolution = vec2f(w, h);
    sr->time = (float)SDL_GetTicks() / 1000.0f;
    sr->camera_pos = vec2f((float)w / 2.0f, -(float)h / 2.0f);
    sr->camera_scale = 1.0f;

    if (pdf->dirty) {
        pdf_viewer_render_page(pdf);
    }

    if (pdf->ready) {
        float margin = 36.0f;
        float available_w = (float)w - margin * 2.0f;
        float available_h = (float)h - margin * 2.0f - FREE_GLYPH_FONT_SIZE * 1.5f;
        float scale_x = available_w / (float)pdf->width;
        float scale_y = available_h / (float)pdf->height;
        float scale = scale_x < scale_y ? scale_x : scale_y;
        if (scale > 1.0f) scale = 1.0f;

        float page_w = (float)pdf->width * scale;
        float page_h = (float)pdf->height * scale;
        float left = ((float)w - page_w) / 2.0f;
        float top = margin + FREE_GLYPH_FONT_SIZE * 1.3f;

        simple_renderer_set_shader(sr, SHADER_FOR_COLOR);
        simple_renderer_solid_rect(sr, vec2f(left - 6.0f, -top + 6.0f), vec2f(page_w + 12.0f, -page_h - 12.0f), hex_to_vec4f(0x070707AA));
        simple_renderer_solid_rect(sr, vec2f(left - 2.0f, -top + 2.0f), vec2f(page_w + 4.0f, -page_h - 4.0f), hex_to_vec4f(0xFFFFFFFF));
        simple_renderer_flush(sr);

        simple_renderer_set_shader(sr, SHADER_FOR_IMAGE);
        glBindTexture(GL_TEXTURE_2D, pdf->texture);
        simple_renderer_image_rect(sr,
                                   vec2f(left, -top),
                                   vec2f(page_w, -page_h),
                                   vec2f(0.0f, 1.0f),
                                   vec2f(1.0f, -1.0f),
                                   vec4fs(1.0f));
        simple_renderer_flush(sr);
    }

    simple_renderer_set_shader(sr, SHADER_FOR_TEXT);
    char title[512];
    snprintf(title, sizeof(title), "PDF page %d: %s", pdf->page, pdf->file_path.items ? pdf->file_path.items : "(nameless omen)");
    pdf_viewer_render_text(atlas, sr, title, vec2f(18.0f, -18.0f), hex_to_vec4f(0xFFDD33FF));
    pdf_viewer_render_text(atlas, sr, pdf->status.items ? pdf->status.items : "", vec2f(18.0f, -18.0f - FREE_GLYPH_FONT_SIZE), hex_to_vec4f(0xCC8C3CFF));
    simple_renderer_flush(sr);
}
