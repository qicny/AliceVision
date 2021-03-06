// This file is part of the AliceVision project.
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "MeshEnergyOpt.hpp"
#include <aliceVision/system/Logger.hpp>

#include <boost/filesystem.hpp>

namespace aliceVision {
namespace mesh {

namespace bfs = boost::filesystem;

MeshEnergyOpt::MeshEnergyOpt(mvsUtils::MultiViewParams* _mp)
    : MeshAnalyze(_mp)
{
//    tmpDir = mp->mvDir + "meshEnergyOpt/";
//    bfs::create_directory(tmpDir);
}

MeshEnergyOpt::~MeshEnergyOpt() = default;

StaticVector<Point3d>* MeshEnergyOpt::computeLaplacianPtsParallel()
{
    StaticVector<Point3d>* lapPts = new StaticVector<Point3d>();
    lapPts->reserve(pts->size());
    lapPts->resize_with(pts->size(), Point3d(0.0f, 0.0f, 0.f));
    int nlabpts = 0;

#pragma omp parallel for
    for(int i = 0; i < pts->size(); i++)
    {
        Point3d lapPt;
        if(getLaplacianSmoothingVector(i, lapPt))
        {
            (*lapPts)[i] = lapPt;
            nlabpts++;
        }
    }

    return lapPts;
}

void MeshEnergyOpt::updateGradientParallel(float lambda, const Point3d& LU,
                                                const Point3d& RD, StaticVectorBool* ptsCanMove)
{
    // printf("nlabpts %i of %i\n",nlabpts,pts->size());
    StaticVector<Point3d>* lapPts = computeLaplacianPtsParallel();

    StaticVector<Point3d>* newPts = new StaticVector<Point3d>();
    newPts->reserve(pts->size());
    newPts->push_back_arr(pts);

#pragma omp parallel for
    for(int i = 0; i < pts->size(); ++i)
    {
        if((ptsCanMove == nullptr) || ((*ptsCanMove)[i]))
        {
            Point3d n;

            if(getBiLaplacianSmoothingVector(i, lapPts, n))
            {
                Point3d p = (*newPts)[i] + n * lambda;
                if((p.x > LU.x) && (p.y > LU.y) && (p.z > LU.z) && (p.x < RD.x) && (p.y < RD.y) && (p.z < RD.z))
                {
                    (*newPts)[i] = p;
                }
            }
        }
    }

    delete pts;
    pts = newPts;
    delete lapPts;
}

bool MeshEnergyOpt::optimizeSmooth(float lambda, int niter, StaticVectorBool* ptsCanMove)
{
    if(pts->size() <= 4)
    {
        return false;
    }

    bool saveDebug = mp ? mp->_ini.get<bool>("meshEnergyOpt.saveAllIterations", false) : false;

    Point3d LU, RD;
    LU = (*pts)[0];
    RD = (*pts)[0];
    for(int i = 0; i < pts->size(); i++)
    {
        LU.x = std::min(LU.x, (*pts)[i].x);
        LU.y = std::min(LU.y, (*pts)[i].y);
        LU.z = std::min(LU.z, (*pts)[i].z);
        RD.x = std::max(RD.x, (*pts)[i].x);
        RD.y = std::max(RD.y, (*pts)[i].y);
        RD.z = std::max(RD.z, (*pts)[i].z);
    }

    ALICEVISION_LOG_INFO("Optimizing mesh smooth: " << std::endl
                         << "\t- lamda: " << lambda << std::endl
                         << "\t- niters: " << niter << std::endl);

    for(int i = 0; i < niter; i++)
    {
        ALICEVISION_LOG_INFO("Optimizing mesh smooth: iteration " << i);
        updateGradientParallel(lambda, LU, RD, ptsCanMove);
        if(saveDebug)
            saveToObj(mp->mvDir + "mesh_smoothed_" + std::to_string(i) + ".obj");
    }

    return true;
}

} // namespace mesh
} // namespace aliceVision
