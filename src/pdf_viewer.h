#ifndef PDF_VIEWER_H_
#define PDF_VIEWER_H_

#include <stdbool.h>

#define GLEW_STATIC
#include <GL/glew.h>

#include <SDL2/SDL.h>

#include "./common.h"
#include "./free_glyph.h"
#include "./simple_renderer.h"

typedef struct {
    String_Builder file_path;
    String_Builder status;
    GLuint texture;
    int width;
    int height;
    int page;
    bool dirty;
    bool ready;
} Pdf_Viewer;

void pdf_viewer_open(Pdf_Viewer *pdf, const char *file_path);
void pdf_viewer_next_page(Pdf_Viewer *pdf);
void pdf_viewer_prev_page(Pdf_Viewer *pdf);
void pdf_viewer_render(Pdf_Viewer *pdf, SDL_Window *window, Free_Glyph_Atlas *atlas, Simple_Renderer *sr);

#endif // PDF_VIEWER_H_
