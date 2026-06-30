#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "include/appstate.h"
#include "include/assets.h"
#include "include/canvas.h"
#include <SDL3_ttf/SDL_ttf.h>

#define CLAY_IMPLEMENTATION
#include "Clay/clay.h"

#define CLAY_PANEL_Z_INDEX 20

enum ButtonType {
    BUTTON_LAYER,
    BUTTON_TOOL
};

typedef struct ButtonContext {
    enum ButtonType type;
    AppState *state;
} ButtonContext;

static inline Clay_Dimensions SDL_MeasureText(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData) {
    TTF_Font **fonts = userData;
    TTF_Font *font = fonts[config->fontId];
    int width = 0, height = 0;

    TTF_SetFontSize(font, config->fontSize);
    
    if (!TTF_GetStringSize(font, text.chars, text.length, &width, &height)) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to measure text: %s", SDL_GetError());
    }

    return (Clay_Dimensions) { (float) width, (float) height };
}

void HandleClayErrors(Clay_ErrorData errorData) {
    SDL_Log("Clay error: %s", errorData.errorText.chars);
}

void HandleButtonInteraction(Clay_ElementId elementId, Clay_PointerData pointerInfo, intptr_t userData) {
    ButtonContext *context = (ButtonContext*)userData;
    if (pointerInfo.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
        context->state->should_redraw = true;
        if (context->type == BUTTON_LAYER) {
            context->state->layers->cur_layer = elementId.offset;
        }
        else if (context->type == BUTTON_TOOL) {
            context->state->layers->current_tool = elementId.offset;
        }
    }
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]){
    SDL_Log("initialization started");
    AppState *state = SDL_calloc(1,sizeof(AppState));

    if (!SDL_CreateWindowAndRenderer("drawing app", 800, 600, SDL_WINDOW_RESIZABLE, &(state->window), &(state->renderer))) {
        SDL_Log("Couldn't create window and renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetRenderDrawBlendMode(state->renderer,SDL_BLENDMODE_BLEND);

    window_for_popups = state->window;

    state->assets = SDL_malloc(sizeof(Assets));
    returnIfNull(state->assets,"Memory allocation error","Failed to allocate memory for assets buffer: %s",SDL_GetError());

    // SDL_Surface *test_surface = loadAsset(asset_please,asset_please_len);
    // state->assets->test_texture = SDL_CreateTextureFromSurface(state->renderer,test_surface);
    // SDL_DestroySurface(test_surface);

    state->layers = SDL_calloc(1,sizeof(Layers));
    returnIfNull(state->layers,"Memory allocation error","Failed to allocate memory for layers buffer: %s",SDL_GetError());
    state->layers->layer_count = 0;
    state->layers->width = 1000;
    state->layers->height = 1000;
    state->layers->edit_layer = createLayer(state->layers->width, state->layers->height, &SDL_calloc);
    addLayer(state->layers,&SDL_realloc,&SDL_calloc);
    fillLayer(&(state->layers->layers[0]),makeColor(255,255,255,255));
    addLayer(state->layers,&SDL_realloc,&SDL_calloc);
    addLayer(state->layers,&SDL_realloc,&SDL_calloc);
    addLayer(state->layers,&SDL_realloc,&SDL_calloc);
    addLayer(state->layers,&SDL_realloc,&SDL_calloc);
    addLayer(state->layers,&SDL_realloc,&SDL_calloc);
    state->layers->cur_layer = state->layers->layer_count-1;

    state->cur_lines = SDL_malloc(sizeof(Lines));
    returnIfNull(state->cur_lines,"Memory allocation error","Failed to allocate memory for lines buffer: %s",SDL_GetError());
    state->cur_lines->point_capacity = 10;
    state->cur_lines->points = SDL_calloc(state->cur_lines->point_capacity,sizeof(SDL_FPoint));
    state->cur_lines->point_count = 0;
    state->cur_lines->is_drawing = false;

    state->canvas_zoom = 0;
    state->canvas_x = 0;
    state->canvas_y = 0;

    uint64_t totalMemorySize = Clay_MinMemorySize();
    Clay_Arena clayMemory = (Clay_Arena) {
        .memory = SDL_malloc(totalMemorySize),
        .capacity = totalMemorySize
    };
    returnIfNull(clayMemory.memory,"Memory allocation error","Failed to allocate memory for Clay: %s",SDL_GetError());

    int width, height;
    SDL_GetWindowSize(state->window, &width, &height);
    Clay_Initialize(clayMemory, (Clay_Dimensions) { (float) width, (float) height }, (Clay_ErrorHandler) { HandleClayErrors });
    
    state->rendererData.renderer = state->renderer;

    if (!TTF_Init()) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Couldn't initialize TTF: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    state->rendererData.textEngine = TTF_CreateRendererTextEngine(state->rendererData.renderer);
    if (!state->rendererData.textEngine) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to create text engine from renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    state->rendererData.fonts = SDL_calloc(1, sizeof(TTF_Font *));
    if (!state->rendererData.fonts) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to allocate memory for the font array: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    TTF_Font *font = loadFontAsset(asset_font_roboto, asset_font_roboto_len, 24.0f);
    if (!font) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to load font: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    state->rendererData.fonts[0] = font;
    
    Clay_SetMeasureTextFunction(SDL_MeasureText, state->rendererData.fonts);

    SDL_GetCurrentRenderOutputSize(state->renderer, &state->screen_width, &state->screen_height);
    Clay_SetLayoutDimensions((Clay_Dimensions) { (float)state->screen_width, (float)state->screen_height });

    state->should_redraw = true;

    *appstate = state;
    return SDL_APP_CONTINUE;
}

#define MAX_TEXTBOX_LEN 256
#define MAX_ACTIVE_TEXTBOXES 10

typedef struct TextboxState {
    char text[MAX_TEXTBOX_LEN];
    size_t length;
    bool is_focused;
    Clay_ElementId id;
} TextboxState;

static TextboxState* active_textboxes[MAX_ACTIVE_TEXTBOXES];
static size_t active_textboxes_count = 0;

static Clay_TextElementConfig textbox_text_config = {
    .fontId = 0,
    .fontSize = 16,
    .textColor = { 255, 255, 255, 255 }
};

void CLAY_TEXTBOX(TextboxState* tb, Clay_ElementId id) {
    tb->id = id;
    
    if (active_textboxes_count < MAX_ACTIVE_TEXTBOXES) {
        active_textboxes[active_textboxes_count++] = tb;
    } else SDL_Log("too many textboxes active at once!");

    CLAY((Clay_ElementDeclaration){
        .id = id,
        .layout = {
            .sizing = {
                .width = CLAY_SIZING_FIXED(60),
                .height = CLAY_SIZING_FIXED(30)
            },
            .padding = {6, 6, 0, 0},
            .childAlignment = { .y = CLAY_ALIGN_Y_CENTER }
        },
        .backgroundColor = tb->is_focused ? (Clay_Color){60, 60, 65, 255} : (Clay_Color){30, 30, 35, 255},
        .border = {
            .color = tb->is_focused ? (Clay_Color){80, 120, 200, 255} : (Clay_Color){80, 80, 85, 255},
            .width = {2, 2, 2, 2}
        },
        .cornerRadius = { 4, 4, 4, 4 }
    }) {
        Clay_String text_str = {
            .chars = tb->text,
            .length = tb->length,
            .isStaticallyAllocated = false
        };
        CLAY_TEXT(text_str, &textbox_text_config);
    }
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event){
    AppState* state = (AppState*)appstate;
    switch (event->type) {
        case SDL_EVENT_QUIT: {
            return SDL_APP_SUCCESS;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            Clay_SetPointerState((Clay_Vector2){ event->button.x, event->button.y }, true);
            state->should_redraw = true;

            bool ui_intercepted = false;
            Clay_ElementIdArray over_ids = Clay_GetPointerOverIds();
            uint32_t root_id = Clay_GetElementId(CLAY_STRING("Clay__RootContainer")).id;
            uint32_t brush_id = Clay_GetElementId(CLAY_STRING("BrushCursor")).id;
            
            for (int i = 0; i < over_ids.length; i++) {
                if (over_ids.internalArray[i].id != root_id && over_ids.internalArray[i].id != brush_id) {
                    ui_intercepted = true;
                    break;
                }
            }

            switch (event->button.button) {
                case SDL_BUTTON_LEFT: {
                    state->mouse1 = true;

                    bool clicked_textbox = false;
                    for (size_t i = 0; i < active_textboxes_count; i++) {
                        TextboxState* tb = active_textboxes[i];
                        Clay_ElementData elData = Clay_GetElementData(tb->id);
                        
                        if (elData.found && isInsideRectangle(event->button.x,event->button.y,elData.boundingBox.x,elData.boundingBox.y,elData.boundingBox.width,elData.boundingBox.height)) {
                            tb->is_focused = true;
                            clicked_textbox = true;
                            SDL_StartTextInput(state->window);
                            state->should_redraw = true;
                        } else if (tb->is_focused) {
                            tb->is_focused = false;
                            state->should_redraw = true;
                        }
                    }
                    if (!clicked_textbox) {
                        SDL_StopTextInput(state->window);
                    }

                    if (ui_intercepted) break;

                    double canvas_x = 0;
                    double canvas_y = 0;
                    screenToCanvas(state,event->button.x,event->button.y,&canvas_x,&canvas_y);
                    if (canvas_x < 0 || canvas_x >= state->layers->width || canvas_y < 0 || canvas_y >= state->layers->height) break;
                    
                    state->cur_lines->is_drawing = true;
                    state->cur_lines->point_count = 1;
                    state->cur_lines->points[0] = (SDL_FPoint){
                        .x = canvas_x,
                        .y = canvas_y
                    };

                    break;
                }
                case SDL_BUTTON_RIGHT: {
                    state->mouse2 = true;
                    break;
                }
                case SDL_BUTTON_MIDDLE: {state->mouse3=true;break;}
            }
            break;
        }
        case SDL_EVENT_MOUSE_MOTION: {
            Clay_SetPointerState((Clay_Vector2){ event->motion.x, event->motion.y }, state->mouse1);

            state->should_redraw = true;

            if (state->mouse3) {
                // drag
                state->canvas_x += event->motion.xrel;
                state->canvas_y += event->motion.yrel;
                state->should_redraw = true;
            }
            else if (state->mouse2){
                // rotate
                state->canvas_rotation -= event->motion.xrel*0.1;
                state->should_redraw = true;
            }
            else if (state->mouse1 && state->cur_lines->is_drawing) {
                double canvas_x = 0;
                double canvas_y = 0;
                screenToCanvas(state,event->motion.x,event->motion.y,&canvas_x,&canvas_y);

                if (state->cur_lines->point_count>=state->cur_lines->point_capacity) break;
                state->cur_lines->points[state->cur_lines->point_count++] = (SDL_FPoint){
                    .x = canvas_x,
                    .y = canvas_y
                };
            }
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            Clay_SetPointerState((Clay_Vector2){ event->button.x, event->button.y }, false);

            switch (event->button.button) {
                case SDL_BUTTON_LEFT: {
                    state->mouse1 = false;
                    if (!state->cur_lines->is_drawing) break;

                    double canvas_x = 0;
                    double canvas_y = 0;
                    screenToCanvas(state,event->button.x,event->button.y,&canvas_x,&canvas_y);

                    if (state->cur_lines->point_count>=state->cur_lines->point_capacity) break;
                    state->cur_lines->points[state->cur_lines->point_count++] = (SDL_FPoint){
                        .x = canvas_x,
                        .y = canvas_y
                    };

                    state->cur_lines->is_drawing = false;
                    state->is_edit_finish = true;
                    break; 
                }
                case SDL_BUTTON_RIGHT: {state->mouse2=false;break;}
                case SDL_BUTTON_MIDDLE: {state->mouse3=false;break;}
            }
            break;
        }
        case SDL_EVENT_MOUSE_WHEEL: {
            float mx, my;
            SDL_GetMouseState(&mx, &my);
            
            Clay_SetPointerState((Clay_Vector2){ mx, my }, state->mouse1);
            
            bool ui_intercepted = false;
            Clay_ElementIdArray over_ids = Clay_GetPointerOverIds();
            uint32_t root_id = Clay_GetElementId(CLAY_STRING("Clay__RootContainer")).id;
            uint32_t brush_id = Clay_GetElementId(CLAY_STRING("BrushCursor")).id;
            
            for (int i = 0; i < over_ids.length; i++) {
                if (over_ids.internalArray[i].id != root_id && over_ids.internalArray[i].id != brush_id) {
                    ui_intercepted = true;
                    break;
                }
            }

            if (ui_intercepted) {
                Clay_UpdateScrollContainers(true, (Clay_Vector2){ event->wheel.x, event->wheel.y * 20.0f }, 0.016f);
                state->should_redraw = true;
                break;
            }

            double cx, cy;
            screenToCanvas(state, mx, my, &cx, &cy);

            state->canvas_zoom += event->wheel.y * 0.1;

            double scale = SDL_pow(2, state->canvas_zoom);
            double rad = state->canvas_rotation * (SDL_PI_F / 180.0);
            double cos_theta = SDL_cos(rad);
            double sin_theta = SDL_sin(rad);

            double sx = cx - (state->layers->width / 2.0);
            double sy = cy - (state->layers->height / 2.0);

            double rx = sx * scale;
            double ry = sy * scale;

            double dx = (rx * cos_theta) - (ry * sin_theta);
            double dy = (rx * sin_theta) + (ry * cos_theta);

            state->canvas_x = mx - dx - (state->screen_width / 2.0);
            state->canvas_y = my - dy - (state->screen_height / 2.0);

            state->should_redraw = true;

            break;
        }
        case SDL_EVENT_WINDOW_RESIZED: {
            // SDL_GetCurrentRenderOutputSize(state->renderer, &state->screen_width, &state->screen_height);
            state->screen_width  = event->window.data1;
            state->screen_height = event->window.data2;
            Clay_SetLayoutDimensions((Clay_Dimensions) { (float)state->screen_width, (float)state->screen_height });
            state->should_redraw = true;
            break;
        }
        case SDL_EVENT_TEXT_INPUT: {
            for (size_t i = 0; i < active_textboxes_count; i++) {
                TextboxState* tb = active_textboxes[i];
                if (tb->is_focused) {
                    size_t add_len = SDL_strlen(event->text.text);
                    if (tb->length + add_len < MAX_TEXTBOX_LEN - 1) {
                        SDL_memcpy(tb->text + tb->length, event->text.text, add_len);
                        tb->length += add_len;
                        tb->text[tb->length] = '\0';
                        state->should_redraw = true;
                    }
                    break;
                }
            }
            break;
        }
        case SDL_EVENT_KEY_DOWN: {
            if (event->key.key == SDLK_BACKSPACE) {
                for (size_t i = 0; i < active_textboxes_count; i++) {
                    TextboxState* tb = active_textboxes[i];
                    if (tb->is_focused && tb->length > 0) {
                        tb->length--;
                        tb->text[tb->length] = '\0';
                        state->should_redraw = true;
                        break;
                    }
                }
            }
            break;
        }
    }
    return SDL_APP_CONTINUE;
}

static Clay_TextElementConfig layer_button_text_config = {
    .fontId = 0,
    .fontSize = 16,
    .textColor = { 255, 255, 255, 255 }
};
static Clay_TextElementConfig tool_button_text_config = {
    .fontId = 0,
    .fontSize = 16,
    .textColor = { 255, 255, 255, 255 },
};

static ButtonContext layer_button_context = {
    .type = BUTTON_LAYER
};
static ButtonContext tool_button_context = {
    .type = BUTTON_TOOL
};

static TextboxState tool_radius_textbox = { 
    .text = "2", 
    .length = 1, 
    .is_focused = false 
};

SDL_AppResult SDL_AppIterate(void *appstate){
    AppState* state = (AppState*)appstate;

    switch (state->layers->current_tool) {
        case TOOL_PEN: {
            state->layers->current_color = makeColor(0, 0, 0, 255);
            state->layers->current_tool_radius = SDL_max(1, SDL_atof(tool_radius_textbox.text));
            break;
        }
        case TOOL_ERASER: {
            state->layers->current_color = makeColor(0, 0, 0, 255);
            state->layers->current_tool_radius = SDL_max(1, SDL_atof(tool_radius_textbox.text));
            break;
        }
    }

    if (drawLinesToLayer(state->cur_lines, &(state->layers->edit_layer),state->layers->current_color,state->layers->current_tool_radius)) state->should_redraw = true;

    if (state->is_edit_finish) {
        state->is_edit_finish = false;
        mergeLayers(&(state->layers->layers[state->layers->cur_layer]), &(state->layers->edit_layer), false, (state->layers->current_tool == TOOL_ERASER));
        fillLayer(&(state->layers->edit_layer), 0);
        state->layers->edit_layer.is_changed = true;
        state->should_redraw = true;
    }

    if (!state->should_redraw) return SDL_APP_CONTINUE;

    SDL_SetRenderDrawColor(state->renderer, 0, 0, 0, 255);
    SDL_RenderClear(state->renderer);

    compositeLayers(state->renderer, state->layers);
    state->should_redraw = false;

    double zoom_factor = SDL_pow(2,state->canvas_zoom);
    SDL_FRect canvas_dest = {
        .w = state->layers->width * zoom_factor,
        .h = state->layers->height * zoom_factor,
    };
    canvas_dest.x = state->canvas_x + state->screen_width/2 - canvas_dest.w/2;
    canvas_dest.y = state->canvas_y + state->screen_height/2 - canvas_dest.h/2;
    SDL_RenderTextureRotated(state->renderer, state->layers->canvas_buffer,NULL,&canvas_dest,state->canvas_rotation,NULL,SDL_FLIP_NONE);

    layer_button_context.state = state;
    tool_button_context.state = state;

    char layers_text[25*state->layers->layer_count];
    
    active_textboxes_count = 0;

    Clay_BeginLayout();

    float mx, my;
    SDL_GetMouseState(&mx, &my);
    
    double scale = SDL_pow(2, state->canvas_zoom);
    float screen_radius = state->layers->current_tool_radius * scale;

    CLAY((Clay_ElementDeclaration){
        .id = CLAY_ID("BrushCursor"),
        .floating = {
            .attachTo = CLAY_ATTACH_TO_ROOT,
            .offset = { mx - screen_radius, my - screen_radius },
        },
        .layout = {
            .sizing = {
                .width = CLAY_SIZING_FIXED(screen_radius * 2),
                .height = CLAY_SIZING_FIXED(screen_radius * 2)
            }
        },
        .border = { 
            .color = {150, 150, 150, 255},
            .width = {.bottom = 2, .left = 2, .right = 2, .top = 2} 
        },
        .cornerRadius = { screen_radius, screen_radius, screen_radius, screen_radius }
    }) {}

    CLAY((Clay_ElementDeclaration){
        .id = CLAY_ID("LayerPanel"),
        .floating = {
            .attachTo = CLAY_ATTACH_TO_ROOT,
            .attachPoints = {
                .element = CLAY_ATTACH_POINT_RIGHT_BOTTOM,
                .parent = CLAY_ATTACH_POINT_RIGHT_BOTTOM
            },
            .offset = {-10, -10}, 
            .zIndex = CLAY_PANEL_Z_INDEX,
        },
        .layout = {
            .sizing = {
                .width = CLAY_SIZING_FIXED(300),
                .height = CLAY_SIZING_FIT(120,400)//CLAY_SIZING_FIXED(400),
            },
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .padding = {10, 10, 10, 10},
            .childGap = 10
        },
        .backgroundColor = {.r=40, .g=40, .b=45, .a=240}
    }) {
        CLAY((Clay_ElementDeclaration){
            .id = CLAY_ID("LayerScrollArea"),
            .layout = {
                .sizing = {
                    .width = CLAY_SIZING_GROW(),
                    .height = CLAY_SIZING_GROW(),
                },
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                .childGap = 8
            },
            .clip = {
                .vertical = true,
                .childOffset = Clay_GetScrollOffset()
            }
        }) {
            for (size_t i = state->layers->layer_count; i-- > 0;) {
                bool is_active = (state->layers->cur_layer == i);
                CLAY((Clay_ElementDeclaration){
                    .id = CLAY_IDI("LayerBtn", (int)i),
                    .layout = {
                        .sizing = {
                            .width = CLAY_SIZING_GROW(),
                            .height = CLAY_SIZING_FIXED(100),
                        },
                        .padding = {5, 5},
                        .childGap = 10,
                        .layoutDirection = CLAY_LEFT_TO_RIGHT,
                        .childAlignment = { .y = CLAY_ALIGN_Y_CENTER }
                    },
                    .backgroundColor = is_active ? (Clay_Color){.r=80, .g=120, .b=200, .a=255} : (Clay_Color){.r=60, .g=60, .b=65, .a=255}
                }) {
                    Clay_OnHover(&HandleButtonInteraction, (intptr_t)(&layer_button_context));
                    
                    CLAY((Clay_ElementDeclaration){
                        .id = CLAY_IDI("LayerPreview", (int)i),
                        .layout = {
                            .sizing = {
                                .width = CLAY_SIZING_FIXED(90),
                                .height = CLAY_SIZING_FIXED(90)
                            },
                        },
                        .image = {
                            .imageData = state->layers->layers[i].texture,
                        },
                        .border = {.color = {.r=0,.g=0,.b=0,.a=255}, .width = {.bottom=2,.left=2,.right=2,.top=2}}
                    }) {}

                    char* cur_layer_text_pointer = layers_text+(i*20);

                    SDL_snprintf(cur_layer_text_pointer,20,"Layer %zu",i);

                    Clay_String cur_clay_string = {
                        .chars=cur_layer_text_pointer,
                        .length=SDL_strnlen(cur_layer_text_pointer,20),
                        .isStaticallyAllocated = false
                    };

                    CLAY_TEXT(cur_clay_string,&layer_button_text_config);
                }
            }
        }
    }

    CLAY((Clay_ElementDeclaration){
        .id = CLAY_ID("ToolPanel"),
        .floating = {
            .attachTo = CLAY_ATTACH_TO_ROOT,
            .attachPoints = {
                .element = CLAY_ATTACH_POINT_LEFT_TOP,
                .parent = CLAY_ATTACH_POINT_LEFT_TOP
            },
            .offset = {10, 50},
            .zIndex = CLAY_PANEL_Z_INDEX,
        },
        .layout = {
            .sizing = {
                .width = CLAY_SIZING_FIXED(100),
                // .height = CLAY_SIZING_FIXED(200),
            },
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .padding = {10, 10, 10, 10},
            .childGap = 10
        },
        .backgroundColor = {.r=40, .g=40, .b=45, .a=240}
    }) {
        for (ToolType cur_tool = 0; cur_tool < TOOL_COUNT; cur_tool++){
            CLAY((Clay_ElementDeclaration){
                .id = CLAY_IDI("ToolBtn", (int)cur_tool),
                .layout = {
                    .sizing = {
                        .width = CLAY_SIZING_GROW(),
                        .height = CLAY_SIZING_FIXED(50),
                    },
                    .padding = {5, 5},
                    .childGap = 10,
                    .layoutDirection = CLAY_LEFT_TO_RIGHT,
                    .childAlignment = { 
                        .x = CLAY_ALIGN_X_CENTER,
                        .y = CLAY_ALIGN_Y_CENTER 
                    }
                },
                .backgroundColor = (state->layers->current_tool==cur_tool) ? (Clay_Color){.r=80, .g=120, .b=200, .a=255} : (Clay_Color){.r=60, .g=60, .b=65, .a=255}
            }) {
                Clay_OnHover(&HandleButtonInteraction, (intptr_t)(&tool_button_context));

                Clay_String cur_tool_clay_string = CLAY_STRING("Unknown");

                switch (cur_tool) {
                    case TOOL_PEN: {cur_tool_clay_string=CLAY_STRING("Pen");break;}
                    case TOOL_ERASER: {cur_tool_clay_string=CLAY_STRING("Eraser");break;}
                }

                CLAY_TEXT(cur_tool_clay_string,&tool_button_text_config);
            }
        }
    }

    CLAY((Clay_ElementDeclaration){
        .id = CLAY_ID("ToolSettingsPanel"),
        .floating = {
            .attachTo = CLAY_ATTACH_TO_ROOT,
            .attachPoints = {
                .element = CLAY_ATTACH_POINT_LEFT_TOP,
                .parent = CLAY_ATTACH_POINT_LEFT_TOP
            },
            .offset = {0, 0},
            .zIndex = CLAY_PANEL_Z_INDEX,
        },
        .layout = {
            .sizing = {
                .width = CLAY_SIZING_GROW(),
                .height = CLAY_SIZING_FIXED(40),
            },
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
            .padding = {10, 10, 10, 10},
            .childGap = 10,
            .childAlignment = { 
                // .x = CLAY_ALIGN_X_CENTER,
                .y = CLAY_ALIGN_Y_CENTER 
            }
        },
        .backgroundColor = {.r=40, .g=40, .b=45, .a=240}
    }) {
        CLAY_TEXT(CLAY_STRING("Radius:"), &tool_button_text_config); 
        CLAY_TEXTBOX(&tool_radius_textbox, CLAY_ID("RadiusTextbox"));
    }

    Clay_RenderCommandArray render_commands = Clay_EndLayout();

    SDL_Clay_RenderClayCommands(&(state->rendererData), &(render_commands));

    SDL_RenderPresent(state->renderer);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result){
    AppState* state = (AppState*)appstate;

}