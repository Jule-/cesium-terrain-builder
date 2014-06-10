#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>

#include "gdal_priv.h"
#include "commander.hpp"

#include "src/TerrainTile.hpp"
#include "src/GlobalGeodetic.hpp"

using namespace std;

class Terrain2Tiff : public Command {
public:
  Terrain2Tiff(const char *name, const char *version) :
    Command(name, version),
    inputFilename(NULL),
    outputFilename(NULL),
    zoom(0),
    tx(0),
    ty(0)
  {}

  static void
  setInputFilename(command_t *command) {
    Terrain2Tiff *self = static_cast<Terrain2Tiff *>(Command::self(command));
    self->inputFilename = command->arg;
    self->membersSet |= TT_INPUT;
  }

  static void
  setOutputFilename(command_t *command) {
    Terrain2Tiff *self = static_cast<Terrain2Tiff *>(Command::self(command));
    self->outputFilename = command->arg;
    self->membersSet |= TT_OUTPUT;
  }

  static void
  setZoomLevel(command_t *command) {
    Terrain2Tiff *self = static_cast<Terrain2Tiff *>(Command::self(command));
    self->zoom = atoi(command->arg);
    self->membersSet |= TT_ZOOM;
  }

  static void
  setTileX(command_t *command) {
    Terrain2Tiff *self = static_cast<Terrain2Tiff *>(Command::self(command));
    self->tx = atoi(command->arg);
    self->membersSet |= TT_TX;
  }

  static void
  setTileY(command_t *command) {
    Terrain2Tiff *self = static_cast<Terrain2Tiff *>(Command::self(command));
    self->ty = atoi(command->arg);
    self->membersSet |= TT_TY;
  }

  void check() const {
    bool failed = false;

    if ((membersSet & TT_INPUT) != TT_INPUT) {
      cerr << "  Error: The input filename must be specified" << endl;
      failed = true;
    }
    if ((membersSet & TT_OUTPUT) != TT_OUTPUT) {
      cerr << "  Error: The output filename must be specified" << endl;
      failed = true;
    }
    if ((membersSet & TT_ZOOM) != TT_ZOOM) {
      cerr << "  Error: The zoom level be specified" << endl;
      failed = true;
    }
    if ((membersSet & TT_TX) != TT_TX) {
      cerr << "  Error: The X tile coordinate must be specified" << endl;
      failed = true;
    }
    if ((membersSet & TT_TY) != TT_TY) {
      cerr << "  Error: The Y tile coordinate must be specified" << endl;
      failed = true;
    }

    if (failed) {
      help();                   // print help and exit
    }
  }

  const char *inputFilename;
  const char *outputFilename;
  unsigned short int zoom;
  unsigned int tx;
  unsigned int ty;

private:
  char membersSet;
  enum members {
    TT_INPUT = 1,               // 2^0, bit 0
    TT_OUTPUT = 2,              // 2^1, bit 1
    TT_ZOOM = 4,                // 2^2, bit 2
    TT_TX = 8,                  // 2^3, bit 3
    TT_TY = 16                  // 2^4, bit 4
  };
};


void terrain2tiff(TerrainTile &terrain, const char *filename, double minx, double miny, double maxx, double maxy) {
  GDALDatasetH hTileDS = terrain.heightsToRaster(minx, miny, maxx, maxy);
  GDALDatasetH hDstDS;
  GDALDriverH hDriver = GDALGetDriverByName("GTiff");

  hDstDS = GDALCreateCopy( hDriver, filename, hTileDS, FALSE,
                           NULL, NULL, NULL );
  if( hDstDS != NULL )
    GDALClose( hDstDS );
  GDALClose( hTileDS );
}

int main(int argc, char *argv[]) {
  TerrainTile terrain;
  Terrain2Tiff command = Terrain2Tiff(argv[0], "0.0.1");

  command.setUsage("-i TERRAIN_FILE -z ZOOM_LEVEL -x TILE_X -y TILE_Y -o OUTPUT_FILE ");
  command.option("-i", "--input-filename <filename>", "the terrain tile file to convert", Terrain2Tiff::setInputFilename);
  command.option("-z", "--zoom-level <int>", "the zoom level represented by the tile", Terrain2Tiff::setZoomLevel);
  command.option("-x", "--tile-x <int>", "the tile x coordinate", Terrain2Tiff::setTileX);
  command.option("-y", "--tile-y <int>", "the tile y coordinate", Terrain2Tiff::setTileY);
  command.option("-o", "--output-filename <filename>", "the output file to create", Terrain2Tiff::setOutputFilename);

  // Parse and check the arguments
  command.parse(argc, argv);
  command.check();

  GDALAllRegister();

  try {
    terrain = TerrainTile(command.inputFilename);
  } catch (int e) {
    switch (e) {
    case 1:
      cerr << "Failed to open " << command.inputFilename << endl;
      return 1;
    case 2:
      cerr << "The file has too many bytes" << endl;
      return 2;
    case 3:
      cerr << "The file does not appear to be a terrain tile" << endl;
      return 3;
    default:
      cerr << "Unknown error: " << e << endl;
      return 4;
    }
  }

  cout << "Creating " << command.outputFilename << " using zoom " << command.zoom << " from tile " << command.tx << "," << command.ty << endl;

  GlobalGeodetic profile;
  double minLon, minLat, maxLon, maxLat;
  profile.tileBounds(command.tx, command.ty, command.zoom, minLon, minLat, maxLon, maxLat);

  terrain2tiff(terrain, command.outputFilename, minLon, minLat, maxLon, maxLat);

  return 0;
}