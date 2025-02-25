/*
 * =====================================================================================
 *
 *       Filename: editormap.cpp
 *        Created: 02/08/2016 22:17:08
 *    Description: EditorMap has no idea of ImageDB, WilImagePackage, etc..
 *                 Use function handler to handle draw, cache, etc
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

#include <memory>
#include <vector>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <functional>
#include <FL/fl_ask.H>

#include "pngf.hpp"
#include "filesys.hpp"
#include "mir2map.hpp"
#include "sysconst.hpp"
#include "mathf.hpp"
#include "editormap.hpp"
#include "mainwindow.hpp"
#include "progressbarwindow.hpp"

EditorMap::EditorMap()
    : m_W(0)
    , m_H(0)
    , m_Valid(false)
    , m_Mir2Map(nullptr)
    , m_Mir2xMapData(nullptr)
    , m_BlockBuf()
{
    std::memset(m_AniSaveTime,  0, sizeof(m_AniSaveTime));
    std::memset(m_AniTileFrame, 0, sizeof(m_AniTileFrame));
}

EditorMap::~EditorMap()
{
    delete m_Mir2Map;
    delete m_Mir2xMapData;
}

void EditorMap::ExtractOneTile(int nX, int nY, std::function<void(uint8_t, uint16_t)> fnWritePNG)
{
    if(true
            &&  Valid()
            &&  ValidC(nX, nY)
            && !(nX % 2)
            && !(nY % 2)
            &&  Tile(nX, nY).Valid){

        auto nFileIndex  = (uint8_t )((Tile(nX, nY).Image & 0X00FF0000) >> 16);
        auto nImageIndex = to_u16((Tile(nX, nY).Image & 0X0000FFFF) >>  0);
        fnWritePNG(nFileIndex, nImageIndex);
    }
}

void EditorMap::ExtractTile(std::function<void(uint8_t, uint16_t)> fnWritePNG)
{
    if(Valid()){
        for(int nXCnt = 0; nXCnt < W(); ++nXCnt){
            for(int nYCnt = 0; nYCnt < H(); ++nYCnt){
                if(!(nXCnt % 2) && !(nYCnt % 2)){
                    ExtractOneTile(nXCnt, nYCnt, fnWritePNG);
                }
            }
        }
    }
}

void EditorMap::DrawLight(int nX, int nY, int nW, int nH, std::function<void(int, int)> fnDrawLight)
{
    if(Valid()){
        for(int nTX = 0; nTX < nW; ++nTX){
            for(int nTY = 0; nTY < nH; ++nTY){
                if(ValidC(nTX + nX, nTY + nY)){
                    auto &rstLight = Light(nTX + nX, nTY + nY);
                    if(rstLight.Valid){
                        fnDrawLight(nTX + nX, nTY + nY);
                    }
                }
            }
        }
    }
}

void EditorMap::DrawTile(int nCX, int nCY, int nCW,  int nCH, std::function<void(uint8_t, uint16_t, int, int)> fnDrawTile)
{
    if(Valid()){
        for(int nY = nCY; nY < nCY + nCH; ++nY){
            for(int nX = nCX; nX < nCX + nCW; ++nX){
                if(!ValidC(nX, nY)){
                    continue;
                }

                if(nX % 2 || nY % 2){
                    continue;
                }

                if(!Tile(nX, nY).Valid){
                    continue;
                }

                auto nFileIndex  = (uint8_t )((Tile(nX, nY).Image & 0X00FF0000) >> 16);
                auto nImageIndex = to_u16((Tile(nX, nY).Image & 0X0000FFFF) >>  0);

                // provide cell-coordinates on map
                // fnDrawTile should convert it to drawarea pixel-coordinates
                fnDrawTile(nFileIndex, nImageIndex, nX, nY);
            }
        }
    }
}

void EditorMap::ExtractOneObject(int nXCnt, int nYCnt, int nIndex, std::function<void(uint8_t, uint16_t, uint32_t)> fnWritePNG)
{
    if(true
            && Valid()
            && ValidC(nXCnt, nYCnt)
            && nIndex >= 0
            && nIndex <= 1){

        auto &rstObj = Object(nXCnt, nYCnt, nIndex);
        if(rstObj.Valid){
            auto nFileIndex  = (uint8_t )((rstObj.Image & 0X00FF0000) >> 16);
            auto nImageIndex = to_u16((rstObj.Image & 0X0000FFFF) >>  0);

            int      nFrameCount = (rstObj.Animated ? rstObj.AniCount : 1);
            uint32_t nImageColor = (rstObj.Alpha ? 0X80FFFFFF : 0XFFFFFFFF);

            for(int nIndex = 0; nIndex < nFrameCount; ++nIndex){
                fnWritePNG(nFileIndex, nImageIndex + to_u16(nIndex), nImageColor);
            }
        }
    }
}

void EditorMap::ExtractObject(std::function<void(uint8_t, uint16_t, uint32_t)> fnWritePNG)
{
    if(Valid()){
        for(int nYCnt = 0; nYCnt < H(); ++nYCnt){
            for(int nXCnt = 0; nXCnt < W(); ++nXCnt){
                ExtractOneObject(nXCnt, nYCnt, 0, fnWritePNG);
                ExtractOneObject(nXCnt, nYCnt, 1, fnWritePNG);
            }
        }
    }
}

void EditorMap::DrawObject(int nCX, int nCY, int nCW, int nCH, bool bGround,
        std::function<void(uint8_t, uint16_t, int, int)> fnDrawObj, std::function<void(int, int)> fnDrawExt)
{
    if(Valid()){
        for(int nY = nCY; nY < nCY + nCH; ++nY){
            for(int nX = nCX; nX < nCX + nCW; ++nX){
                // // we should draw actors, extensions here, but I delay it after the over-ground
                // // object drawing, it's really wired but works
                // if(!bGround){ fnDrawExt(nX, nY); }

                // 2. regular draw
                if(ValidC(nX, nY)){
                    for(int nIndex = 0; nIndex < 2; ++nIndex){
                        auto &rstObj = Object(nX, nY, nIndex);
                        if(true
                                && rstObj.Valid
                                && rstObj.Ground == bGround){

                            auto nFileIndex  = (uint8_t )((rstObj.Image & 0X00FF0000) >> 16);
                            auto nImageIndex = to_u16((rstObj.Image & 0X0000FFFF) >>  0);
                            auto nAnimated   = (bool    )((rstObj.Animated));
                            auto nAniType    = (uint8_t )((rstObj.AniType ));
                            auto nAniCount   = (uint8_t )((rstObj.AniCount));

                            if(nAnimated){
                                if(false
                                        || nFileIndex == 11
                                        || nFileIndex == 26
                                        || nFileIndex == 41
                                        || nFileIndex == 56
                                        || nFileIndex == 71){

                                    nImageIndex += to_u16(m_AniTileFrame[nAniType][nAniCount]);
                                }
                            }

                            fnDrawObj(nFileIndex, nImageIndex, nX, nY);
                        }
                    }
                }

                // put the actors, extensions rendering here, it's really really wired but
                // works, tricky part for mir2 resource maker
                // if(!bGround){ fnDrawExt(nX, nY); }
            }

            for(int nX = nCX; nX < nCX + nCW; ++nX){
                if(!bGround){ fnDrawExt(nX, nY); }
            }
        }
    }
}

void EditorMap::UpdateFrame(int nLoopTime)
{
    // m_AniTileFrame[i][j]:
    //   i: denotes how fast the animation is.
    //   j: denotes how many frames the animation has.

    if(Valid()){
        const static uint32_t nDelayMS[] = {150, 200, 250, 300, 350, 400, 420, 450};
        for(int nCnt = 0; nCnt < 8; ++nCnt){
            m_AniSaveTime[nCnt] += nLoopTime;
            if(m_AniSaveTime[nCnt] > nDelayMS[nCnt]){
                for(int nFrame = 0; nFrame < 16; ++nFrame){
                    m_AniTileFrame[nCnt][nFrame]++;
                    if(m_AniTileFrame[nCnt][nFrame] >= nFrame){
                        m_AniTileFrame[nCnt][nFrame] = 0;
                    }
                }
                m_AniSaveTime[nCnt] = 0;
            }
        }
    }
}

bool EditorMap::Resize(
        int nX, int nY, int nW, int nH, // define a region on original map
        int nNewX, int nNewY,           // where the cropped region start on new map
        int nNewW, int nNewH)           // size of new map
{
    // we only support 2M * 2N cropping and expansion
    if(Valid()){
        nX = (std::max<int>)(0, nX);
        nY = (std::max<int>)(0, nY);

        if(nX % 2){ nX--; nW++; }
        if(nY % 2){ nY--; nH++; }
        if(nW % 2){ nW++; }
        if(nH % 2){ nH++; }

        nW = (std::min<int>)(nW, W() - nX);
        nH = (std::min<int>)(nH, H() - nY);

        if(true
                && nX == 0
                && nY == 0
                && nW == W()
                && nH == H()
                && nNewX == 0
                && nNewY == 0
                && nNewW == nW
                && nNewH == nH){
            // return if nothing need to do
            return true;
        }

        if(nNewW <= 0 || nNewH <= 0){
            return false;
        }

        // region is not empty
        // OK now we have a practical job to do

        auto stOldBlockBuf = m_BlockBuf;
        MakeBuf(nNewW, nNewH);

        for(int nTY = 0; nTY < nH; ++nTY){
            for(int nTX = 0; nTX < nW; ++nTX){
                int nSrcX = nTX + nX;
                int nSrcY = nTY + nY;
                int nDstX = nTX + nNewX;
                int nDstY = nTY + nNewY;

                if(true
                        && nDstX >= 0 && nDstX < nNewW
                        && nDstY >= 0 && nDstY < nNewH){

                    if(true
                            && !(nDstX % 2) && !(nDstY % 2)
                            && !(nSrcX % 2) && !(nSrcY % 2)){
                        m_BlockBuf[nDstX / 2][nDstY / 2] = stOldBlockBuf[nSrcX / 2][nSrcY / 2];
                    }
                }
            }
        }

        m_Valid = true;
        m_W     = nNewW;
        m_H     = nNewH;

        return true;
    }
    return false;
}

bool EditorMap::LoadMir2Map(const char *szFullName)
{
    delete m_Mir2Map     ; m_Mir2Map      = new Mir2Map();
    delete m_Mir2xMapData; m_Mir2xMapData = nullptr;

    if(m_Mir2Map->Load(szFullName)){
        MakeBuf(m_Mir2Map->W(), m_Mir2Map->H());
        InitBuf();
    }

    delete m_Mir2xMapData; m_Mir2xMapData = nullptr;
    delete m_Mir2xMapData; m_Mir2xMapData = nullptr;

    return Valid();
}

bool EditorMap::LoadMir2xMapData(const char *szFullName)
{
    delete m_Mir2Map     ; m_Mir2Map      = nullptr;
    delete m_Mir2xMapData; m_Mir2xMapData = new Mir2xMapData();

    if(!m_Mir2xMapData->Load(szFullName)){
        MakeBuf(m_Mir2xMapData->W(), m_Mir2xMapData->H());
        InitBuf();
    }

    delete m_Mir2xMapData; m_Mir2xMapData = nullptr;
    delete m_Mir2xMapData; m_Mir2xMapData = nullptr;

    return Valid();
}

void EditorMap::Optimize()
{
    if(!Valid()){ return; }

    // try to remove some unnecessary tile/cell
    // tile
    for(int nY = 0; nY < H(); ++nY){
        for(int nX = 0; nX < W(); ++nX){
            OptimizeTile(nX, nY);
            OptimizeCell(nX, nY);
        }
    }
}

void EditorMap::OptimizeTile(int, int)
{
}

void EditorMap::OptimizeCell(int, int)
{
}

void EditorMap::ClearBuf()
{
    m_W = 0;
    m_H = 0;

    m_Valid = false;
    m_BlockBuf.clear();
}

bool EditorMap::InitBuf()
{
    int nW = 0;
    int nH = 0;

    m_Valid = false;

    if(m_Mir2Map && m_Mir2Map->Valid()){
        nW = m_Mir2Map->W();
        nH = m_Mir2Map->H();
    }else if(m_Mir2xMapData && m_Mir2xMapData->Valid()){
        nW = m_Mir2xMapData->W();
        nH = m_Mir2xMapData->H();
    }else{
        return false;
    }

    for(int nY = 0; nY < nH; ++nY){
        for(int nX = 0; nX < nW; ++nX){
            if(!(nX % 2) && !(nY % 2)){
                SetBufTile(nX, nY);
            }

            SetBufLight(nX, nY);
            SetBufGround(nX, nY);

            SetBufObj(nX, nY, 0);
            SetBufObj(nX, nY, 1);
        }
    }

    m_W     = nW;
    m_H     = nH;
    m_Valid = true;

    return true;
}

void EditorMap::MakeBuf(int nW, int nH)
{
    // make a buffer for loading new map
    // or extend / crop old map
    ClearBuf();
    if(nW == 0 || nH == 0 || nW % 2 || nH % 2){ return; }

    m_BlockBuf.resize(nW / 2);
    for(auto &rstBuf: m_BlockBuf){
        rstBuf.resize(nH / 2);
        std::memset(&(rstBuf[0]), 0, rstBuf.size() * sizeof(rstBuf[0]));
    }
}

void EditorMap::SetBufTile(int nX, int nY)
{
    // don't check Valid() and ValidC()
    // called in InitBuf() and where Valid() is not set yet

    if(true
            && !(nX % 2) && !(nY % 2)

            && nX >= 0
            && nX / 2 < to_d(m_BlockBuf.size())

            && nY >= 0
            && nY / 2 < to_d(m_BlockBuf[nX / 2].size())){

        if(m_Mir2Map && m_Mir2Map->Valid()){
            extern ImageDB g_ImageDB;
            if(m_Mir2Map->TileValid(nX, nY, g_ImageDB)){
                Tile(nX, nY).Valid = true;
                Tile(nX, nY).Image = m_Mir2Map->Tile(nX, nY) & 0X00FFFFFF;
            }
        }else if(m_Mir2xMapData && m_Mir2xMapData->Valid()){
            if(m_Mir2xMapData->Tile(nX, nY).Param & 0X80000000){
                Tile(nX, nY).Valid = true;
                Tile(nX, nY).Image = m_Mir2xMapData->Tile(nX, nY).Param & 0X00FFFFFF;
            }
        }
    }
}

void EditorMap::SetBufGround(int nX, int nY)
{
    if(true
            && nX >= 0
            && nX / 2 < to_d(m_BlockBuf.size())

            && nY >= 0
            && nY / 2 < to_d(m_BlockBuf[nX / 2].size())){

        bool    bCanFly    = false;
        bool    bCanWalk   = false;
        uint8_t nAttribute = 0;

        if(m_Mir2Map && m_Mir2Map->Valid()){
            if(m_Mir2Map->GroundValid(nX, nY)){
                bCanFly    = true;
                bCanWalk   = true;
                nAttribute = 0;
            }
        }else if(m_Mir2xMapData && m_Mir2xMapData->Valid()){
            uint8_t nLandByte = m_Mir2xMapData->Cell(nX, nY).LandByte();
            bCanWalk   = (nLandByte & 0X80) ? true : false;
            bCanFly    = (nLandByte & 0X40) ? true : false;
            nAttribute = (nLandByte & 0X3F);
        }

        Cell(nX, nY).CanFly    = bCanFly;
        Cell(nX, nY).CanWalk   = bCanWalk;
        Cell(nX, nY).LandType = nAttribute;
    }
}

void EditorMap::SetBufObj(int nX, int nY, int nIndex)
{
    if(true
            && nIndex >= 0
            && nIndex <= 1

            && nX >= 0
            && nX / 2 < to_d(m_BlockBuf.size())

            && nY >= 0
            && nY / 2 < to_d(m_BlockBuf[nX / 2].size())){

        uint32_t nObj       = 0;
        bool     bObjValid  = false;
        bool     bGroundObj = false;
        bool     bAniObj    = false;
        bool     bAlphaObj  = false;
        uint8_t  nAniType   = 0;
        uint8_t  nAniCount  = 0;

        if(m_Mir2Map && m_Mir2Map->Valid()){
            // mir2 map
            // animation info is in Mir2Map::Object() at higher 8 bits
            extern ImageDB g_ImageDB;
            if(m_Mir2Map->ObjectValid(nX, nY, nIndex, g_ImageDB)){

                // I checked the code
                // here the mir 3 checked CELLINFO::bFlag

                bObjValid = true;
                if(m_Mir2Map->GroundObjectValid(nX, nY, nIndex, g_ImageDB)){
                    bGroundObj = true;
                }
                if(m_Mir2Map->AniObjectValid(nX, nY, nIndex, g_ImageDB)){
                    bAniObj = true;
                }

                // [31:24] : animation info
                // [23:16] : file index
                // [15: 0] : image index

                auto nObjDesc = m_Mir2Map->Object(nX, nY, nIndex);
                nAniType  = (uint8_t )((nObjDesc & 0X70000000) >> (4 + 8 + 16));
                nAniCount = (uint8_t )((nObjDesc & 0X0F000000) >> (0 + 8 + 16));
                nObj      = to_u32((nObjDesc & 0X00FFFFFF));

                // in mir2map if an object is not animated
                // then it shouldn't be alpha-blended, check GameProc.cpp
                if(bAniObj){
                    bAlphaObj = ((nObj & 0X80000000) ? true : false);
                }else{
                    bAlphaObj = false;
                    nAniType  = 0;
                    nAniCount = 0;
                }
            }
        }else if(m_Mir2xMapData && m_Mir2xMapData->Valid()){
            auto stArray = m_Mir2xMapData->Cell(nX, nY).ObjectArray(nIndex);
            if(stArray[4] & 0X80){
                bObjValid = true;
                nObj = 0
                    | ((to_u32(stArray[2])) << 16)
                    | ((to_u32(stArray[1])) <<  8)
                    | ((to_u32(stArray[0])) <<  0);

                if(stArray[4] & 0X01){
                    bGroundObj = true;
                }

                if(stArray[3] & 0X80){
                    bAniObj   = true;
                    nAniType  = ((stArray[3] & 0X70) >> 4);
                    nAniCount = ((stArray[3] & 0X0F) >> 0);
                }

                if(stArray[4] & 0X02){
                    bAlphaObj = true;
                }
            }
        }

        Object(nX, nY, nIndex).Valid    = bObjValid;
        Object(nX, nY, nIndex).Alpha    = bAlphaObj;
        Object(nX, nY, nIndex).Ground   = bGroundObj;
        Object(nX, nY, nIndex).Animated = bAniObj;
        Object(nX, nY, nIndex).AniType  = nAniType;
        Object(nX, nY, nIndex).AniCount = nAniCount;
        Object(nX, nY, nIndex).Image    = nObj;
    }
}

void EditorMap::SetBufLight(int nX, int nY)
{
    if(true
            && nX >= 0
            && nX / 2 < to_d(m_BlockBuf.size())

            && nY >= 0
            && nY / 2 < to_d(m_BlockBuf[nX / 2].size())){

        if(m_Mir2Map && m_Mir2Map->Valid()){
            if(m_Mir2Map->LightValid(nX, nY)){
                Light(nX, nY).Valid  = true;
                Light(nX, nY).Color  = 0;
                Light(nX, nY).Alpha  = 0;
                Light(nX, nY).Radius = 0;
            }
        }else if(m_Mir2xMapData && m_Mir2xMapData->Valid()){
            auto nLightByte = m_Mir2xMapData->Cell(nX, nY).LightByte();
            if(nLightByte & 0X80){
                Light(nX, nY).Valid  = true;
                Light(nX, nY).Color  = 0;
                Light(nX, nY).Alpha  = 0;
                Light(nX, nY).Radius = 0;
            }
        }
    }
}

void EditorMap::DrawSelectGround(int nX, int nY, int nW, int nH, std::function<void(int, int, int)> fnDrawSelectGround)
{
    if(Valid()){
        for(int nTX = nX; nTX < nX + nW; ++nTX){
            for(int nTY = nY; nTY < nY + nH; ++nTY){
                for(int nIndex = 0; nIndex < 4; ++nIndex){
                    if(ValidC(nTX, nTY) && Cell(nTX, nTY).SelectConf.Ground){
                        fnDrawSelectGround(nX, nY, nIndex);
                    }
                }
            }
        }
    }
}

void EditorMap::ClearGroundSelect()
{
    if(Valid()){
        for(int nX = 0; nX < W(); ++nX){
            for(int nY = 0; nY < H(); ++nY){
                for(int nIndex = 0; nIndex < 4; ++nIndex){
                    Cell(nX, nY).SelectConf.Ground = false;
                }
            }
        }
    }
}

bool EditorMap::SaveMir2xMapData(const char *szFullName)
{
    if(!Valid()){
        fl_alert("%s", "Invalid editor map!");
        return false;
    }

    Mir2xMapData stMapData;
    stMapData.Allocate(W(), H());

    for(int nX = 0; nX < W(); ++nX){
        for(int nY = 0; nY < H(); ++nY){

            // tile
            if(!(nX % 2) && !(nY % 2)){
                stMapData.Tile(nX, nY).Param = Tile(nX, nY).MakeU32();
            }

            // cell
            auto &rstDstCell = stMapData.Cell(nX, nY);
            std::memset(&rstDstCell, 0, sizeof(rstDstCell));

            // cell::land
            rstDstCell.Param |= ((to_u32(Cell(nX, nY).MakeLandU8())) << 16);

            // cell::light
            rstDstCell.Param |= ((to_u32(Light(nX, nY).MakeU8())) << 8);

            // cell::obj[0]
            {
                auto stArray = Object(nX, nY, 0).MakeArray();
                if(stArray[4] & 0X80){
                    rstDstCell.Obj[0].Param = 0X80000000
                        | ((to_u32(stArray[2])) << 16)
                        | ((to_u32(stArray[1])) <<  8)
                        | ((to_u32(stArray[0])) <<  0);
                    rstDstCell.ObjParam |= ((to_u32(stArray[3] & 0XFF)) << 8);
                    rstDstCell.ObjParam |= ((to_u32(stArray[4] & 0X03)) << 6);
                }else{
                    rstDstCell.Obj[0].Param = 0;
                    rstDstCell.ObjParam &= 0XFFFF0000;
                }
            }

            // cell::obj[1]
            {
                auto stArray = Object(nX, nY, 1).MakeArray();
                if(stArray[4] & 0X80){
                    rstDstCell.Obj[1].Param = 0X80000000
                        | ((to_u32(stArray[2])) << 16)
                        | ((to_u32(stArray[1])) <<  8)
                        | ((to_u32(stArray[0])) <<  0);
                    rstDstCell.ObjParam |= ((to_u32(stArray[3] & 0XFF)) << 24);
                    rstDstCell.ObjParam |= ((to_u32(stArray[4] & 0X03)) << 22);
                }else{
                    rstDstCell.Obj[1].Param = 0;
                    rstDstCell.ObjParam &= 0X0000FFFF;
                }
            }
        }
    }

    return stMapData.Save(szFullName) ? false : true;
}

void EditorMap::ExportOverview(std::function<void(uint8_t, uint16_t, int, int, bool)> fnExportOverview)
{
    if(Valid()){
        int nCountAll = W() * H() * 3;
        auto fnUpdateProgressBar = [nCountAll, nLastPercent = 0](int nCurrCount) mutable
        {
            auto nPercent = std::lround(100.0 * nCurrCount / nCountAll);
            if(nPercent > nLastPercent){
                // 1. record percent
                nLastPercent = nPercent;

                // 2. update progress bar
                extern ProgressBarWindow *g_ProgressBarWindow;
                g_ProgressBarWindow->SetValue(nPercent);
                g_ProgressBarWindow->Redraw();
                g_ProgressBarWindow->ShowAll();
                Fl::check();
            }
        };

        int nCount = 0;
        for(int nX = 0; nX < W(); ++nX){
            for(int nY = 0; nY < H(); ++nY){
                fnUpdateProgressBar(nCount++);
                if(true
                        && !(nX % 2)
                        && !(nY % 2)){

                    auto &rstTile = Tile(nX, nY);
                    if(rstTile.Valid){
                        fnExportOverview((rstTile.Image & 0X00FF0000) >> 16, (rstTile.Image & 0X0000FFFF), nX, nY, false);
                    }
                }
            }
        }

        for(int nX = 0; nX < W(); ++nX){
            for(int nY = 0; nY < H(); ++nY){
                fnUpdateProgressBar(nCount++);
                for(int nIndex = 0; nIndex < 2; ++nIndex){
                    auto rstObj = Object(nX, nY, nIndex);
                    if(true
                            && rstObj.Valid
                            && rstObj.Ground){
                        fnExportOverview((rstObj.Image & 0X00FF0000) >> 16, (rstObj.Image & 0X0000FFFF), nX, nY, true);
                    }
                }
            }
        }

        for(int nX = 0; nX < W(); ++nX){
            for(int nY = 0; nY < H(); ++nY){
                fnUpdateProgressBar(nCount++);
                for(int nIndex = 0; nIndex < 2; ++nIndex){
                    auto rstObj = Object(nX, nY, nIndex);
                    if(true
                            &&  rstObj.Valid
                            && !rstObj.Ground){
                        fnExportOverview((rstObj.Image & 0X00FF0000) >> 16, (rstObj.Image & 0X0000FFFF), nX, nY, true);
                    }
                }
            }
        }

        extern ProgressBarWindow *g_ProgressBarWindow;
        g_ProgressBarWindow->HideAll();
    }
}

EditorMap *EditorMap::ExportLayer()
{
    if(true
            &&  Valid()
            && !(W() % 2)
            && !(H() % 2)){

        int nX0 =  W();
        int nY0 =  H();
        int nX1 = -1;
        int nY1 = -1;

        auto pEditorMap = new EditorMap();
        pEditorMap->Allocate(W(), H());

        auto fnExtend = [&nX0, &nY0, &nX1, &nY1](int nX, int nY)
        {
            nX0 = (std::min<int>)(nX0, nX);
            nY0 = (std::min<int>)(nY0, nY);
            nX1 = (std::max<int>)(nX1, nX);
            nY1 = (std::max<int>)(nY1, nY);
        };

        extern LayerBrowserWindow *g_LayerBrowserWindow;
        for(int nX = 0; nX < W(); ++nX){
            for(int nY = 0; nY < H(); ++nY){

                // 1. tile
                if(g_LayerBrowserWindow->ImportTile()){
                    if(true
                            && !(nX % 2)
                            && !(nY % 2)){
                        auto &rstTile = Tile(nX, nY);
                        if(true
                                && rstTile.Valid
                                && rstTile.SelectConf.Tile){
                            pEditorMap->Tile(nX, nY) = rstTile;
                            fnExtend(nX, nY);
                        }
                    }
                }

                // 2. object, two layers
                for(int bGroundObj = 0; bGroundObj < 2; ++bGroundObj){
                    if(g_LayerBrowserWindow->ImportObject((bool)(bGroundObj))){
                        for(int nIndex = 0; nIndex < 2; ++nIndex){
                            auto &rstCell = Cell(nX, nY);
                            if(rstCell.Obj[nIndex].Valid){
                                if(false
                                        || ( bGroundObj &&  rstCell.Obj[nIndex].Ground && rstCell.SelectConf.GroundObj)
                                        || (!bGroundObj && !rstCell.Obj[nIndex].Ground && rstCell.SelectConf.OverGroundObj)){

                                    for(int nValidSlotIndex = 0; nValidSlotIndex < 2; ++nValidSlotIndex){
                                        if(!pEditorMap->Object(nX, nY, nValidSlotIndex).Valid){
                                            pEditorMap->Object(nX, nY, nValidSlotIndex) = rstCell.Obj[nIndex];
                                            fnExtend(nX, nY);
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // 3. light
            }
        }

        if(true
                && ValidC(nX0, nY0)
                && ValidC(nX1, nY1)){

            nX0 = (nX0 / 2) * 2;
            nY0 = (nY0 / 2) * 2;

            int nW = ((nX1 - nX0 + 1 + 1) / 2) * 2;
            int nH = ((nY1 - nY0 + 1 + 1) / 2) * 2;

            if(pEditorMap->Resize(nX0, nY0, nW, nH, 0, 0, nW, nH)){
                return pEditorMap;
            }
        }

        delete pEditorMap; pEditorMap = nullptr;
    }

    return nullptr;
}

bool EditorMap::Allocate(int nW, int nH)
{
    if(true
            && !(nW % 2)
            && !(nH % 2)){

        m_W     = nW;
        m_H     = nH;
        m_Valid = true;

        std::memset(m_AniSaveTime,  0, sizeof(m_AniSaveTime));
        std::memset(m_AniTileFrame, 0, sizeof(m_AniTileFrame));

        m_Mir2Map      = nullptr;
        m_Mir2xMapData = nullptr;

        m_BlockBuf.resize(nW / 2);
        for(auto &rstBuf: m_BlockBuf){
            rstBuf.resize(nH / 2);
            std::memset(&(rstBuf[0]), 0, rstBuf.size() * sizeof(rstBuf[0]));
        }
        return true;
    }
    return false;
}
