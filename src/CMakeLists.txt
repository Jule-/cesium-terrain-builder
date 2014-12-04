# Ensure we have a unified GDAL: `GDALOpenEx` is only present after
# unification.
include(CheckLibraryExists)
check_library_exists(gdal GDALOpenEx "gdal.h" HAVE_UNIFIED_GDAL)
if(NOT MSVC AND NOT HAVE_UNIFIED_GDAL)
  message(FATAL_ERROR "The GDAL version must be one that implements RFC 46 (GDAL/OGR unification) i.e. >= 2.0.0")
endif()

# We need zlib
find_package(ZLIB)
if(NOT ZLIB_FOUND)
  message(FATAL_ERROR "The zlib library cannot be found on the sytem")
endif()
include_directories(${ZLIB_INCLUDE_DIRS})

# We need boost
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.57.0 COMPONENTS system filesystem regex)
if(NOT Boost_FOUND)
  message(FATAL_ERROR "The boost library cannot be found on the sytem")
endif()
include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})

add_library(ctb SHARED
  GDALTile.cpp
  GDALTiler.cpp
  TerrainDataset.cpp
  TerrainTiler.cpp
  TerrainTile.cpp
  GlobalMercator.cpp
  GlobalGeodetic.cpp)
target_link_libraries(ctb ${GDAL_LIBRARIES} ${ZLIB_LIBRARIES} ${Boost_LIBRARIES})

# Install libctb
set(HEADERS
  Bounds.hpp
  Coordinate.hpp
  GDALTile.hpp
  GDALTiler.hpp
  GlobalGeodetic.hpp
  GlobalMercator.hpp
  Grid.hpp
  GridIterator.hpp
  RasterIterator.hpp
  RasterTiler.hpp
  CTBException.hpp
  TerrainIterator.hpp
  TerrainTile.hpp
  TerrainTiler.hpp
  TerrainDataset.hpp
  Tile.hpp
  TileCoordinate.hpp
  TilerIterator.hpp
  types.hpp)
install(FILES ${HEADERS} DESTINATION include/ctb)
install(FILES ctb.hpp DESTINATION include)

install(TARGETS ctb DESTINATION lib)

if(MSVC)
  # On Windows in order to use CTB without modifying PATH
  # we need to keep dll collocated.
  install(TARGETS ctb DESTINATION bin)
endif()
