#include <pebble.h>

static Window *window;
static TextLayer *text_layer;
static int sBlackness = 50;

static Layer *s_grad_face;
static GRect s_grad_face_bounds;
static uint8_t s_butState;

static Animation *s_moveAnim = NULL;
static AnimationImplementation s_moveAnimImpl;

/*
static uint8_t dither[] = {
  0b00000000, // 0
  0b00010001,
  0b00100100,
  0b01010101,
  0b11011011,
  0b11101110,
  0b11111111 // 6
};*/

//------------------------------------------------------------------------------
// ANIMATION
//------------------------------------------------------------------------------
// typedef void (*AnimationUpdateImplementation)(struct Animation *animation,
// const uint32_t time_normalized);
static void claire_anim_update(Animation *animation,
                               const uint32_t time_normalized) {
  layer_mark_dirty(s_grad_face);
}

//------------------------------------------------------------------------------
// Rendering
//------------------------------------------------------------------------------
static void face_grad_update_proc(Layer *layer, GContext *ctx) {
  GBitmap *rBuf = graphics_capture_frame_buffer_format(ctx, GBitmapFormat1Bit);

  uint16_t stride = gbitmap_get_bytes_per_row(rBuf);
  GRect box = gbitmap_get_bounds(rBuf);
  uint8_t *dat = gbitmap_get_data(rBuf);
  //

  uint8_t col = 0;  // dither[0];
  for (int c = box.size.h - 1; c >= 0; --c) {
    if (c > sBlackness)
      col = 0b00000000;
    else
      col = 0b11111111;

    for (int x = stride - 1; x >= 0; --x) {
      dat[c * stride + x] = col;
    }
  }

  if (graphics_release_frame_buffer(ctx, rBuf)) {
    // Todo
  } else {
    text_layer_set_text(text_layer, "FAIL");
  }

  if (s_butState != 0) {
    sBlackness = sBlackness + (s_butState >> 2) - (s_butState & 0b001);
    if (sBlackness < 0) sBlackness = 0;
    if (sBlackness >= (box.size.h - 1)) sBlackness = box.size.h - 1;

    animation_schedule(s_moveAnim);
  } else {
    animation_unschedule(s_moveAnim);
  }
}

//------------------------------------------------------------------------------
// INPUT HANDLING
//------------------------------------------------------------------------------
static void up_down_handler(ClickRecognizerRef recognizer, void *context) {
  text_layer_set_text(text_layer, "Up");
  s_butState |= 0b100;
  layer_mark_dirty(s_grad_face);
}

static void up_up_handler(ClickRecognizerRef recognizer, void *context) {
  s_butState &= 0b011;
  layer_mark_dirty(s_grad_face);
}

static void select_down_handler(ClickRecognizerRef recognizer, void *context) {
  text_layer_set_text(text_layer, "Select");
  s_butState |= 0b010;
  layer_mark_dirty(s_grad_face);
}

static void select_up_handler(ClickRecognizerRef recognizer, void *context) {
  s_butState &= 0b101;
  layer_mark_dirty(s_grad_face);
}

static void down_down_handler(ClickRecognizerRef recognizer, void *context) {
  text_layer_set_text(text_layer, "Down");
  s_butState |= 0b001;
  layer_mark_dirty(s_grad_face);
}

static void down_up_handler(ClickRecognizerRef recognizer, void *context) {
  s_butState &= 0b110;
  layer_mark_dirty(s_grad_face);
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
}

static void window_unload(Window *window) {
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

  // Setup Animation
  s_moveAnimImpl.setup = NULL;
  s_moveAnimImpl.update = claire_anim_update;
  s_moveAnimImpl.teardown = NULL;

  s_moveAnim = animation_create();
  animation_set_implementation(s_moveAnim, &s_moveAnimImpl);
  animation_set_delay(s_moveAnim, 33);
  animation_set_duration(s_moveAnim, ANIMATION_DURATION_INFINITE);

  sBlackness = 50;
}

static void deinit(void) {
  animation_unschedule_all();
  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}