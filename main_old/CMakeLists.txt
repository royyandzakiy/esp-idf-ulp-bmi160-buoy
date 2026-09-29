idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS ""
    REQUIRES ulp esp_driver_gpio
)

set(ulp_app_name ulp_${COMPONENT_NAME})
set(ulp_riscv_sources "ulp/ulp_main.c")
set(ulp_exp_dep_srcs "main.c")

ulp_embed_binary(${ulp_app_name} "${ulp_riscv_sources}" "${ulp_exp_dep_srcs}")
