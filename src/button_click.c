#include <pebble.h>

static Window *window;
static TextLayer *text_layer;
static int sBlackness = 50;

static Layer *s_grad_face;
static GRect s_grad_face_bounds;
static uint8_t s_butState;

static Animation *s_moveAnim = NULL;
static AnimationImplementation s_moveAnimImpl;

#ifdef PBL_COLOR
// ARGB
static uint8_t hue_shift_left(uint8_t in) {
  return (in & 0b11000000) | (((in << 1) | ((in & 0b00111111) >> 5)) & 0b00111111);
}

static uint8_t hue_shift_right(uint8_t in) {
  return (in & 0b11000000) | (((in >> 1) | ((in & 0b00000001) << 5)) & 0b00111111);
}

static uint8_t s_basecol = 0b11000011;
#else  
  static uint8_t dither[] = {
  0b00000000, // 0
  0b00010001,
  0b00100100,
  0b01010101,
  0b11011011,
  0b11101110,
  0b11111111 // 6
};
#endif

//------------------------------------------------------------------------------
// ANIMATION - Game LOOP
//------------------------------------------------------------------------------
void claire_anim_update(Animation *animation, const AnimationProgress progress) {
  if(s_butState == 0)
    return; // No Change
  
  sBlackness = sBlackness - (s_butState >> 2) + (s_butState & 0b001);
  if (sBlackness < 0) sBlackness = 0;
  if (sBlackness >= (s_grad_face_bounds.size.h - 1)) sBlackness = s_grad_face_bounds.size.h - 1;
  layer_mark_dirty(s_grad_face);
}

//------------------------------------------------------------------------------
// Rendering
//------------------------------------------------------------------------------
#ifdef PBL_COLOR
static void face_grad_update_proc(Layer *layer, GContext *ctx) {
  GBitmap *rBuf = graphics_capture_frame_buffer_format(ctx, GBitmapFormat8Bit);

  uint16_t stride = gbitmap_get_bytes_per_row(rBuf);
  GRect box = gbitmap_get_bounds(rBuf);
  uint8_t *dat = gbitmap_get_data(rBuf);
  //

  uint8_t col = s_basecol;
  for (int c = box.size.h - 1; c >= 0; --c) {
    int a = c-sBlackness;
    if((a < 0) && ((a % 5)==0))
      col = hue_shift_left(col);
    
    for (int x = stride - 1; x >= 0; --x) {
      dat[c * stride + x] = col;
    }
  }

  if (graphics_release_frame_buffer(ctx, rBuf)) {
    // Todo
  } else {
    text_layer_set_text(text_layer, "FAIL");
  }
}
#else
static void face_grad_update_proc(Layer *layer, GContext *ctx) {
  GBitmap *rBuf = graphics_capture_frame_buffer_format(ctx, GBitmapFormat1Bit);

  uint16_t stride = gbitmap_get_bytes_per_row(rBuf);
  GRect box = gbitmap_get_bounds(rBuf);
  uint8_t *dat = gbitmap_get_data(rBuf);
  //

  uint8_t col = dither[0];
  for (int c = box.size.h - 1; c >= 0; --c) {
    int a = c-sBlackness;
    if(a < 0)
      col = dither[0];
    else if (a > 6)
      col = dither[6];
    else
      col = dither[a];
    
    for (int x = stride - 1; x >= 0; --x) {
      dat[c * stride + x] = col;
    }
  }

  if (graphics_release_frame_buffer(ctx, rBuf)) {
    // Todo
  } else {
    text_layer_set_text(text_layer, "FAIL");
  }
}
#endif
//------------------------------------------------------------------------------
// INPUT HANDLING
//------------------------------------------------------------------------------
static void up_down_handler(ClickRecognizerRef recognizer, void *context) {
  text_layer_set_text(text_layer, "Up");
  s_butState |= 0b100;
}

static void up_up_handler(ClickRecognizerRef recognizer, void *context) {
  s_butState &= 0b011;
}

static void select_down_handler(ClickRecognizerRef recognizer, void *context) {
  text_layer_set_text(text_layer, "Select");
  s_butState |= 0b010;
}

static void select_up_handler(ClickRecognizerRef recognizer, void *context) {
  s_butState &= 0b101;
}

static void down_down_handler(ClickRecognizerRef recognizer, void *context) {
  text_layer_set_text(text_layer, "Down");
  s_butState |= 0b001;

}

static void down_up_handler(ClickRecognizerRef recognizer, void *context) {
  s_butState &= 0b110;

}

static void click_config_provider(void *context) {
  window_raw_click_subscribe(BUTTON_ID_UP, up_down_handler, up_up_handler,
                             context);
  window_raw_click_subscribe(BUTTON_ID_SELECT, select_down_handler,
                             select_up_handler, context);
  window_raw_click_subscribe(BUTTON_ID_DOWN, down_down_handler, down_up_handler,
                             context);
}

//------------------------------------------------------------------------------
// SETUP
//------------------------------------------------------------------------------
static void window_load(Window *window) {
  // Setup Layers
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_grad_face = layer_create(bounds);
  s_grad_face_bounds = layer_get_bounds(s_grad_face);
  layer_set_update_proc(s_grad_face, face_grad_update_proc);

  text_layer = text_layer_create(
      (GRect){.origin = {0, 72}, .size = {bounds.size.w, 20}});
  text_layer_set_text(text_layer, "Press a button");
  text_layer_set_text_alignment(text_layer, GTextAlignmentCenter);

  layer_add_child(window_layer, s_grad_face);
  layer_add_child(s_grad_face, text_layer_get_layer(text_layer));
  
  // Setup Animation
  s_moveAnimImpl.setup = NULL;
  s_moveAnimImpl.update = claire_anim_update;
  s_moveAnimImpl.teardown = NULL;

  s_moveAnim = animation_create();
  animation_set_implementation(s_moveAnim, &s_moveAnimImpl);
  animation_set_duration(s_moveAnim, ANIMATION_DURATION_INFINITE);
  
  animation_schedule(s_moveAnim);
}

static void window_unload(Window *window) {
  animation_unschedule_all();
  animation_destroy(s_moveAnim);
  text_layer_destroy(text_layer);
  layer_destroy(s_grad_face);
}

static void init(void) {
  // Create Window
  window = window_create();
  window_set_click_config_provider(window, click_config_provider);
  window_set_window_handlers(window,
                             (WindowHandlers){
                                 .load = window_load, .unload = window_unload,
                             });

  window_stack_push(window, false);
}

static void deinit(void) {
  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}