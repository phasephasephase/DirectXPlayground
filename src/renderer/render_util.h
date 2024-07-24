#pragma once
#include "../common.h"

#include <d2d1_3.h>

// basic vector struct
typedef struct vec2_t {
    float x, y;
    
    explicit vec2_t(float x = 0.0f, float y = 0.0f) {
        this->x = x;
        this->y = y;
    }
} vec2_t;

void init_render(IDXGISurface* back_buffer);
void init_render_11on12(IDXGISwapChain3* swap_chain, ID3D11On12Device* device11on12);
void deinit_render();

void begin_render(int index = 0);
void end_render(int index = 0);

void draw_rect(vec2_t pos, vec2_t size, D2D1::ColorF color, float thickness = 1.0f);
void draw_filled_rect(vec2_t pos, vec2_t size, D2D1::ColorF color);
void draw_line(vec2_t start, vec2_t end, D2D1::ColorF color, float thickness = 1.0f);

void set_font(const wchar_t* font_name);
void draw_text(const wchar_t* text, vec2_t pos, D2D1::ColorF color, bool shadow = false, float size = 12.0f);
float text_width(const wchar_t* text, float size = 12.0f);
float text_height(const wchar_t* text, float size = 12.0f);

