#ifndef CTBTERRAINDATASET_HPP
#define CTBTERRAINDATASET_HPP

/**
* @file TerrainDataset.hpp
* @brief This declares the `TerrainDataset` class
*/

#include <boost\filesystem.hpp>
#include "config.hpp"           // for CTB_DLL

namespace ctb {
  class TerrainDataset;
}


class CTB_DLL ctb::TerrainDataset
{
public:
  TerrainDataset();
  TerrainDataset(std::string rootDirectory);

  int getMinLevel() {
    return this->mMinLevel;
  }
  int getMaxLevel() {
    return this->mMaxLevel;
  }

  int getMinX(int level) {
    return this->mMinX;
  }
  int getMaxX(int level) {
    return this->mMaxX;
  }

  int getMinY(int level) {
    return this->mMinY;
  }
  int getMaxY(int level) {
    return this->mMaxY;
  }

  std::string getTileFilename(int level, int x, int y) {
    return static_cast<std::ostringstream*>(&(std::ostringstream() << this->mRootDirectory << "\\" << level << "\\" << x << "\\" << y << ".terrain"))->str();
  }

  virtual ~TerrainDataset();

private:
  static void findMinMax(boost::filesystem::path& p, int& min, int& max);

  std::string mRootDirectory;

  int mMinLevel;
  int mMaxLevel;

  int mMinX;
  int mMaxX;

  int mMinY;
  int mMaxY;
};

#endif /* CTBTERRAINDATASET_HPP */
