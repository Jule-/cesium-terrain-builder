
/**
* @file TerrainDataset.cpp
* @brief This defines the `TerrainDataset` class
*/

#include <iostream>
#include <string>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

#include "cpl_vsi.h"

#include "config.hpp"
#include "CTBException.hpp"
#include "TerrainDataset.hpp"

using namespace std;
using namespace boost::filesystem;
using namespace ctb;

TerrainDataset::TerrainDataset()
: mMinLevel(0)
, mMaxLevel(0)
, mMinX(0)
, mMaxX(0)
, mMinY(0)
, mMaxY(0)
{}

TerrainDataset::TerrainDataset(string rootDirectory)
: mMinLevel(0)
, mMaxLevel(0)
, mMinX(0)
, mMaxX(0)
, mMinY(0)
, mMaxY(0)
, mRootDirectory(rootDirectory)
{
  path p(rootDirectory);

  try {
    TerrainDataset::findMinMax(p, this->mMinLevel, this->mMaxLevel);

    if (this->mMinLevel >= 0) {
      p /= to_string(this->mMinLevel);
      TerrainDataset::findMinMax(p, this->mMinX, this->mMaxX);
    }

    if (this->mMinX >= 0) {
      p /= to_string(this->mMinX);
      TerrainDataset::findMinMax(p, this->mMinY, this->mMaxY);
    }
  }
  catch (const filesystem_error& ex) {
    cerr << ex.what() << endl;
  }
}

void TerrainDataset::findMinMax(boost::filesystem::path& p, int& min, int& max)
{
  min = -1;
  max = -1;

  if (exists(p)) {
    if (is_directory(p)) {
      typedef vector<path> vec;
      vec v;

      copy(directory_iterator(p), directory_iterator(), back_inserter(v));
      sort(v.begin(), v.end());

      for (vec::const_iterator it(v.begin()); it != v.end(); ++it) {

        int i;
        try {
          i = boost::lexical_cast<int>((*it).stem().string());
          if (min == -1) {
            min = i;
          }
          if (i < min) {
            min = i;
          }
          if (i > max) {
            max = i;
          }
        }
        catch (boost::bad_lexical_cast & e) {
          //cout << "Exception caught : " << e.what() << endl;
        }
      }

      if (min == -1) {
        throw CTBException("Could not find TMS like directory tree.");
      }
    }
    else {
      throw CTBException((p.string() + " exists, but is not a directory.").c_str());
    }
  }
  else {
    throw CTBException((p.string() + " does not exist.").c_str());
  }
}

TerrainDataset::~TerrainDataset()
{}
