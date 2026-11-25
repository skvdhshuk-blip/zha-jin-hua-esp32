idf_component_register(
    SRCS "main.c"
         "zjh_app.c"
         "zjh_ai.c"
         "zjh_audio.c"
         "zjh_cards.c"
         "zjh_clock.c"
         "zjh_hand.c"
         "zjh_round.c"
         "zjh_store.c"
         "zjh_ui.c"
         "generated/zjh_images.c"
         "generated/zjh_pcm.c"
         "generated/zjh_font_16.c"
         "generated/zjh_font_20.c"
         "generated/zjh_font_28.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_timer nvs_flash
)
