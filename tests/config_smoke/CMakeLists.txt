# The settings file, parsed without a game: pure text in, values out.

add_executable(config_smoke test_config.c)
target_link_libraries(config_smoke PRIVATE xbox_config)
if(MSVC)
    target_compile_options(config_smoke PRIVATE /W4)
else()
    target_compile_options(config_smoke PRIVATE -Wall -Wextra)
endif()
