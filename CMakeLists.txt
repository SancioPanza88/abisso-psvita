# ABISSO - Port PSVita (SDL2). Build dual: Vita (VPK) se toolchain Vita,
# altrimenti eseguibile PC per test.
cmake_minimum_required(VERSION 3.16)

if(DEFINED ENV{VITASDK})
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
endif()

project(abisso C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(CMAKE_TOOLCHAIN_FILE MATCHES "vita.toolchain.cmake" OR DEFINED ENV{VITASDK} AND CMAKE_SYSTEM_NAME STREQUAL "Vita")
  set(BUILD_VITA TRUE)
else()
  # Euristica: se esiste VITASDK e il compilatore e' arm-vita-eabi -> Vita
  if(DEFINED ENV{VITASDK})
    set(BUILD_VITA TRUE)
  else()
    set(BUILD_VITA FALSE)
  endif()
endif()

if(BUILD_VITA)
  include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)
  add_definitions(-DABISSO_VITA)
endif()

set(VITA_APP_NAME "Abisso")
set(VITA_TITLEID "ABSS00001")
set(VITA_VERSION "01.00")

set(SRC
  src/main.c
  src/data.c
  src/game.c
  src/render.c
  src/audio.c
  src/input.c
)

add_executable(${PROJECT_NAME} ${SRC})

find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)

target_include_directories(${PROJECT_NAME} PRIVATE src)
if(TARGET SDL2::SDL2)
  target_link_libraries(${PROJECT_NAME} SDL2::SDL2)
else()
  target_include_directories(${PROJECT_NAME} PRIVATE ${SDL2_INCLUDE_DIRS})
  target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES})
endif()
if(TARGET SDL2_image::SDL2_image)
  target_link_libraries(${PROJECT_NAME} SDL2_image::SDL2_image)
else()
  target_link_libraries(${PROJECT_NAME} SDL2_image)
endif()
target_link_libraries(${PROJECT_NAME} m)
# SDL2_image statica su Vita richiede le sue dipendenze (png/jpeg/webp/z):
# collegale esplicitamente se presenti nel sysroot (innocuo se transitive).
if(BUILD_VITA)
  foreach(_lname png16 png jpeg webpdemux webp sharpyuv pthread z)
    find_library(_found_${_lname} ${_lname}
      PATHS "$ENV{VITASDK}/arm-vita-eabi/lib" NO_DEFAULT_PATH)
    if(_found_${_lname})
      target_link_libraries(${PROJECT_NAME} ${_found_${_lname}})
    endif()
  endforeach()
endif()

if(BUILD_VITA)
  # NB: SDL2/SDL2_image di vdpm portano gia le dipendenze Vita (Sce* stub)
  # in modo transitivo: non aggiungere stub manuali (nomi errati = link error).
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -Wall -Wno-unused-function -Wno-unused-variable")
  vita_create_self(eboot.bin ${PROJECT_NAME} UNSAFE)
  # Asset: includi tutte le PNG + livearea
  file(GLOB ASSET_SPRITES "assets/sprites/*.png")
  file(GLOB ASSET_SPECIALI "assets/speciali/*.png")
  set(VPK_FILES "")
  foreach(f ${ASSET_SPRITES})
    get_filename_component(fn ${f} NAME)
    list(APPEND VPK_FILES FILE ${f} assets/sprites/${fn})
  endforeach()
  foreach(f ${ASSET_SPECIALI})
    get_filename_component(fn ${f} NAME)
    list(APPEND VPK_FILES FILE ${f} assets/speciali/${fn})
  endforeach()
  vita_create_vpk(abisso.vpk ${VITA_TITLEID} eboot.bin
    VERSION ${VITA_VERSION}
    NAME ${VITA_APP_NAME}
    FILE sce_sys/icon0.png sce_sys/icon0.png
    FILE sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
    FILE sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
    FILE sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
    ${VPK_FILES}
  )
else()
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -Wall -Wno-unused-function -Wno-unused-variable")
endif()
