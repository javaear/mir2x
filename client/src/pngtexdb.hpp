/*
 * =====================================================================================
 *
 *       Filename: pngtexdb.hpp
 *        Created: 02/26/2016 21:48:43
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
#include <vector>
#include <memory>
#include <unordered_map>

#include "zsdb.hpp"
#include "inndb.hpp"
#include "hexstr.hpp"
#include "sdldevice.hpp"

struct PNGTexEntry
{
    SDL_Texture *Texture;
};

class PNGTexDB: public innDB<uint32_t, PNGTexEntry>
{
    private:
        std::unique_ptr<ZSDB> m_zsdbPtr;

    public:
        PNGTexDB(size_t nResMax)
            : innDB<uint32_t, PNGTexEntry>(nResMax)
            , m_zsdbPtr()
        {}

    public:
        bool Load(const char *szPNGTexDBName)
        {
            try{
                m_zsdbPtr = std::make_unique<ZSDB>(szPNGTexDBName);
            }catch(...){
                return false;
            }
            return true;
        }

    public:
        SDL_Texture *Retrieve(uint32_t nKey)
        {
            if(PNGTexEntry stEntry {nullptr}; this->RetrieveResource(nKey, &stEntry)){
                return stEntry.Texture;
            }
            return nullptr;
        }

        SDL_Texture *Retrieve(uint8_t nIndex, uint16_t nImage)
        {
            return Retrieve(to_u32((to_u32(nIndex) << 16) + nImage));
        }

    public:
        virtual std::tuple<PNGTexEntry, size_t> loadResource(uint32_t nKey)
        {
            char szKeyString[16];
            PNGTexEntry stEntry {nullptr};

            if(std::vector<uint8_t> stBuf; m_zsdbPtr->decomp(hexstr::to_string<uint32_t, 4>(nKey, szKeyString, true), 8, &stBuf)){
                extern SDLDevice *g_sdlDevice; // TODO
                stEntry.Texture = g_sdlDevice->createTexture(stBuf.data(), stBuf.size());
            }
            return {stEntry, stEntry.Texture ? 1 : 0};
        }

        virtual void freeResource(PNGTexEntry &rstEntry)
        {
            if(rstEntry.Texture){
                SDL_DestroyTexture(rstEntry.Texture);
                rstEntry.Texture = nullptr;
            }
        }
};
