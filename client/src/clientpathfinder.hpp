/*
 * =====================================================================================
 *
 *       Filename: clientpathfinder.hpp
 *        Created: 03/28/2017 21:13:11
 *    Description:
 *
 *        Version: 1.0
 *       Revision: none
 *       Compiler: gcc
 *
 *         Author: ANHONG
 *          Email: anhonghe@gmail.com
 *   Organization: USTC
 *
 * =====================================================================================
 */

#pragma once
#include <map>
#include "pathfinder.hpp"

class ClientPathFinder final: public AStarPathFinder
{
    private:
        friend class ProcessRun;

    private:
        const bool m_checkGround;

    private:
        const int m_checkCreature;

    private:
        mutable std::map<uint64_t, int> m_cache;

    public:
        ClientPathFinder(bool, int, int);

    private:
        int getGrid(int, int) const;
};
