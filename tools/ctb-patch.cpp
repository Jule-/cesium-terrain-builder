/**
 * @file ctb-patch.cpp
 * @brief The terrain patch tool
 *
 * This tool takes a terrain directory and patch children switch on all tiles.
 */

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <mutex>

#include <boost/filesystem.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>

#include "gdal_priv.h"
#include "commander.hpp"

#include "config.hpp"
#include "CTBException.hpp"
#include "TerrainTile.hpp"
#include "TerrainDataset.hpp"
#include "GlobalGeodetic.hpp"

using namespace std;
using namespace boost::filesystem;
using namespace ctb;

/// Handle the terrain patch CLI options
class TerrainPatch : public Command {
public:
  TerrainPatch(const char *name, const char *version) :
    Command(name, version),
    inputDirectory("."),
    outputDirectory("./patched"),
    simulate(false),
    verbose(false),
    quiet(false)
  {}

  static void
    setAuto(command_t *command) {
      TerrainPatch *self = static_cast<TerrainPatch *>(Command::self(command));
      self->membersSet |= TT_INPUT;
      self->membersSet |= TT_OUTPUT;
  }

  static void
    setInputDirectory(command_t *command) {
      TerrainPatch *self = static_cast<TerrainPatch *>(Command::self(command));
      self->inputDirectory = command->arg;
      self->membersSet |= TT_INPUT;
  }

  static void
    setOutputDirectory(command_t *command) {
      TerrainPatch *self = static_cast<TerrainPatch *>(Command::self(command));
      self->outputDirectory = command->arg;
      self->membersSet |= TT_OUTPUT;
  }

  static void
    setSimulate(command_t *command) {
      TerrainPatch *self = static_cast<TerrainPatch *>(Command::self(command));
      self->simulate = true;
      self->membersSet |= TT_SIMULATE;
  }

  static void
    setVerbose(command_t *command) {
      TerrainPatch *self = static_cast<TerrainPatch *>(Command::self(command));
      self->verbose = true;
      self->membersSet |= TT_VERBOSE;
  }

  static void
    setQuiet(command_t *command) {
      TerrainPatch *self = static_cast<TerrainPatch *>(Command::self(command));
      self->quiet = true;
      self->membersSet |= TT_QUIET;
  }

  void
    check() const {
      bool failed = false;

      if (!this->additionalArgs().empty()) {
        auto args = this->additionalArgs();
        ostringstream oss;
        copy(args.begin(), args.end() - 1, ostream_iterator<const char*>(oss, ","));
        oss << args.back();
        cerr << "  Error: Unrecognized arguments: " << oss.str() << endl;
        failed = true;
      }

      if ((membersSet & TT_INPUT) != TT_INPUT) {
        cerr << "  Error: The input directory must be specified." << endl;
        failed = true;
      }
      if ((membersSet & TT_OUTPUT) != TT_OUTPUT) {
        cerr << "  Error: The output directory must be specified." << endl;
        failed = true;
      }

      if (this->verbose) {
        const_cast<TerrainPatch*>(this)->log = boost::bind(&TerrainPatch::consoleLog, this, _1, _2);
        const_cast<TerrainPatch*>(this)->info = boost::bind(&TerrainPatch::consoleSimpleLog, this, _1);
        const_cast<TerrainPatch*>(this)->logPatch = boost::bind(&TerrainPatch::consoleLogPatch, this, _1, _2);
      }
      else {
        const_cast<TerrainPatch*>(this)->log = &TerrainPatch::noLog;
        const_cast<TerrainPatch*>(this)->info = boost::bind(&TerrainPatch::noLog, "", _1);
        const_cast<TerrainPatch*>(this)->logPatch = &TerrainPatch::noLogPatch;
      }
      if (this->quiet) {
        const_cast<TerrainPatch*>(this)->warn = boost::bind(&TerrainPatch::noLog, "", _1);
        const_cast<TerrainPatch*>(this)->err = boost::bind(&TerrainPatch::noLog, "", _1);
      }
      else {
        const_cast<TerrainPatch*>(this)->warn = boost::bind(&TerrainPatch::consoleLog, this, "Warning", _1);
        const_cast<TerrainPatch*>(this)->err = boost::bind(&TerrainPatch::consoleLog, this, "Error", _1);
      }

      if (failed) {
        help();                   // print help and exit
      }
  }

  boost::function<void(string, string)> log;
  boost::function<void(string)> info;
  boost::function<void(string)> warn;
  boost::function<void(string)> err;
  boost::function<void(string, TerrainTile)> logPatch;

  string inputDirectory;
  string outputDirectory;
  bool simulate;
  bool verbose;
  bool quiet;

private:
  char membersSet;
  enum members {
    TT_INPUT = 1,               // 2^0, bit 0
    TT_OUTPUT = 2,              // 2^1, bit 1
    TT_SIMULATE = 4,            // 2^2, bit 2
    TT_VERBOSE = 8,             // 2^3, bit 3
    TT_QUIET = 16               // 2^4, bit 4
  };

  static void consoleLog(const TerrainPatch *command, string label, string message) {
    cout << "[" << label << "] " << message << endl;
  }

  static void consoleSimpleLog(const TerrainPatch *command, string message) {
    cout << message << endl;
  }

  static void noLog(string label, string message) {}

  static void consoleLogPatch(const TerrainPatch *command, string label, TerrainTile &tile) {
    if (command->verbose) {
      cout << "[" << label << "] z: " << tile.zoom << " x: " << tile.x << " y: " << tile.y << endl;
    }
  }

  static void noLogPatch(string label, TerrainTile tile) {}
};

int tileCount = 0;
int tilePatchedCount = 0;
TerrainPatch *command = NULL;

/**
* Create a filename for a tile coordinate
*
* This also creates the tile directory structure.
*/
static string
getTileFilename(const TileCoordinate *coord, const path& dir, const char *extension) {
  static mutex mutex;
  path filename(dir);
  filename /= to_string(coord->zoom);
  filename /= to_string(coord->x);

  lock_guard<std::mutex> lock(mutex);

  if (!command->simulate) {
    // Ensure the `{zoom}/{x}` directory exists
    create_directories(filename);
  }

  // Create the filename itself, adding the extension if required
  filename /= to_string(coord->y);
  if (extension != NULL) {
    filename.replace_extension(extension);
  }

  return filename.string();
}

void
patchTerrainTile(TerrainDataset &terrain, TerrainTile &tile, const path &outputDirectory) {
  bool needPatch = false;

  tileCount++;

  // NE
  try {
    TerrainTile *childNE = new TerrainTile(terrain.getTileFilename(tile.zoom + 1, tile.x << 1, tile.y << 1).c_str(), TileCoordinate(tile.zoom + 1, tile.x << 1, tile.y << 1));
    if (!tile.hasChildNE()) {
      command->logPatch("+NE", tile);
      tile.setChildNE();
      needPatch = true;
    }
    patchTerrainTile(terrain, *childNE, outputDirectory);
  }
  catch (CTBException) {
    if (tile.hasChildNE()) {
      command->logPatch("-NE", tile);
      tile.setChildNE(false);
      needPatch = true;
    }
  }

  // NW
  try {
    TerrainTile *childNW = new TerrainTile(terrain.getTileFilename(tile.zoom + 1, (tile.x << 1) + 1, tile.y << 1).c_str(), TileCoordinate(tile.zoom + 1, (tile.x << 1) + 1, tile.y << 1));
    if (!tile.hasChildNW()) {
      command->logPatch("+NW", tile);
      tile.setChildNW();
      needPatch = true;
    }
    patchTerrainTile(terrain, *childNW, outputDirectory);
  }
  catch (CTBException) {
    if (tile.hasChildNW()) {
      command->logPatch("-NW", tile);
      tile.setChildNW(false);
      needPatch = true;
    }
  }

  // SE
  try {
    TerrainTile *childSE = new TerrainTile(terrain.getTileFilename(tile.zoom + 1, tile.x << 1, (tile.y << 1) + 1).c_str(), TileCoordinate(tile.zoom + 1, tile.x << 1, (tile.y << 1) + 1));
    if (!tile.hasChildSE()) {
      command->logPatch("+SE", tile);
      tile.setChildSE();
      needPatch = true;
    }
    patchTerrainTile(terrain, *childSE, outputDirectory);
  }
  catch (CTBException) {
    if (tile.hasChildSE()) {
      command->logPatch("-SE", tile);
      tile.setChildSE(false);
      needPatch = true;
    }
  }

  // SW
  try {
    TerrainTile *childSW = new TerrainTile(terrain.getTileFilename(tile.zoom + 1, (tile.x << 1) + 1, (tile.y << 1) + 1).c_str(), TileCoordinate(tile.zoom + 1, (tile.x << 1) + 1, (tile.y << 1) + 1));
    if (!tile.hasChildSW()) {
      command->logPatch("+SW", tile);
      tile.setChildSW();
      needPatch = true;
    }
    patchTerrainTile(terrain, *childSW, outputDirectory);
  }
  catch (CTBException) {
    if (tile.hasChildSW()) {
      command->logPatch("-SW", tile);
      tile.setChildSW(false);
      needPatch = true;
    }
  }

  if (needPatch) {
    tilePatchedCount++;
  }

  string filename = getTileFilename(&tile, outputDirectory, "terrain");
  if (!command->simulate) {
    tile.writeFile(filename.c_str());
  }
}

/**
* Patch the terrain with good children switches
*/
void
patchTerrain(TerrainDataset &terrain, const string &outputDirectory) {
  path outputDir(outputDirectory);
  int minLevel = terrain.getMinLevel();

  if (minLevel != 0) {
    stringstream stream;
    stream << "Terrain start level is '" << minLevel << "'. (Upper tree from level '0' must be present for Cesium to work)";
    command->warn(stream.str());
  }

  // Force BBox to maximum emprise for level 0 in order to warn for missing tiles.
  int minX = (minLevel == 0) ? 0 : terrain.getMinX(minLevel);
  int maxX = (minLevel == 0) ? 1 : terrain.getMaxX(minLevel);
  int minY = (minLevel == 0) ? 0 : terrain.getMinY(minLevel);
  int maxY = (minLevel == 0) ? 0 : terrain.getMaxY(minLevel);

  for (int x = minX; x <= maxX; ++x) {
    for (int y = minY; y <= maxY; ++y) {
      string file = terrain.getTileFilename(minLevel, x, y);
      try {
        TerrainTile tile(file.c_str(), TileCoordinate(minLevel, x, y));
        patchTerrainTile(terrain, tile, outputDir);
      }
      catch (CTBException e) {
        if (minLevel == 0) {
          stringstream stream;
          stream << file << " tile does not exist. (Must be present for Cesium to work)";
          command->warn(stream.str());
        }
      }
    }
  }
}

int
main(int argc, char *argv[]) {
  // Setup cout locale
  cout.imbue(locale(setlocale(LC_ALL, "")));

  // Setup the command interface
  command = new TerrainPatch(argv[0], version.cstr);

  command->setUsage("[options] (--auto|--input-directory <directory> --output-directory <directory>)");
  command->option("-a", "--auto", "use '.' as input directory and './patched' as output directory", TerrainPatch::setAuto);
  command->option("-i", "--input-directory <directory>", "the terrain root directory to convert", TerrainPatch::setInputDirectory);
  command->option("-o", "--output-directory <directory>", "the output root directory to create", TerrainPatch::setOutputDirectory);
  command->option("-s", "--simulate", "simulate patching, no file will be written", TerrainPatch::setSimulate);
  command->option("-v", "--verbose", "output patched tiles", TerrainPatch::setVerbose);
  command->option("-q", "--quiet", "no output", TerrainPatch::setQuiet);

  // Parse and check the arguments
  command->parse(argc, argv);
  command->check();

  try {
    {
      stringstream stream;
      stream
        << "Patching:\n"
        << "        in: " << command->inputDirectory << endl
        << "       out: " << command->outputDirectory;
      command->info(stream.str());
    }

    if (!command->simulate) {
      // Ensure that the `{outputDirectory}` directory exists
      boost::filesystem::create_directories(command->outputDirectory);
    }

    GDALAllRegister();

    // Instantiate an appropriate terrain dataset
    TerrainDataset dataset(command->inputDirectory);

    {
      stringstream stream;
      stream
        << "  zoom min: " << dataset.getMinLevel() << endl
        << "  zoom max: " << dataset.getMaxLevel();
      command->info(stream.str());
    }

    // Write the data to tiff
    patchTerrain(dataset, command->outputDirectory);

    // Report
    cout << "\nTile patched: " << tilePatchedCount << "/" << tileCount << endl;
  }
  catch (CTBException e) {
    command->err(e.what());
  }
  catch (boost::filesystem::filesystem_error e) {
    command->err(e.code().message());
  }

#ifdef _DEBUG
  system("pause");
#endif

  return 0;
}
