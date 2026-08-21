#ifndef TEST_ASCII_WORLD_H
#define TEST_ASCII_WORLD_H

#include "test.h"

#include <base/dbg.h>
#include <base/str.h>

#include <engine/shared/datafile.h>
#include <engine/shared/map.h>
#include <engine/storage.h>

#include <game/collision.h>
#include <game/gamecore.h>
#include <game/layers.h>
#include <game/mapitems.h>

#include <initializer_list>
#include <memory>
#include <vector>

// A CCollision built from ASCII rows through the real map loading path:
// '#' is solid, anything else is air, and one character is one 32x32 tile.
class CAsciiWorld
{
public:
	explicit CAsciiWorld(std::initializer_list<const char *> Rows)
	{
		m_pStorage = m_TestInfo.CreateTestStorage();
		dbg_assert(m_pStorage != nullptr, "test storage");

		const int Height = Rows.size();
		const int Width = str_length(*Rows.begin());
		std::vector<CTile> vTiles((size_t)Width * Height, CTile{TILE_AIR, 0, 0, 0});
		int y = 0;
		for(const char *pRow : Rows)
		{
			dbg_assert(str_length(pRow) == Width, "every row must have the same width");
			for(int x = 0; x < Width; x++)
				vTiles[(size_t)y * Width + x].m_Index = pRow[x] == '#' ? TILE_SOLID : TILE_AIR;
			y++;
		}

		CDataFileWriter Writer;
		dbg_assert(Writer.Open(m_pStorage.get(), m_TestInfo.m_aFilename), "open test map for writing");

		const int TilesData = Writer.AddData(vTiles.size() * sizeof(CTile), vTiles.data());

		CMapItemVersion Version;
		Version.m_Version = 1;
		Writer.AddItem(MAPITEMTYPE_VERSION, 0, sizeof(Version), &Version);

		CMapItemLayerTilemap Tilemap{};
		Tilemap.m_Layer.m_Type = LAYERTYPE_TILES;
		Tilemap.m_Version = 3;
		Tilemap.m_Width = Width;
		Tilemap.m_Height = Height;
		Tilemap.m_Flags = TILESLAYERFLAG_GAME;
		Tilemap.m_Color = CColor{255, 255, 255, 255};
		Tilemap.m_ColorEnv = -1;
		Tilemap.m_Image = -1;
		Tilemap.m_Data = TilesData;
		StrToInts(Tilemap.m_aName, std::size(Tilemap.m_aName), "Game");
		Tilemap.m_Tele = Tilemap.m_Speedup = Tilemap.m_Front = Tilemap.m_Switch = Tilemap.m_Tune = -1;
		Writer.AddItem(MAPITEMTYPE_LAYER, 0, sizeof(Tilemap), &Tilemap);

		CMapItemGroup Group{};
		Group.m_Version = 3;
		Group.m_ParallaxX = 100;
		Group.m_ParallaxY = 100;
		Group.m_NumLayers = 1;
		StrToInts(Group.m_aName, std::size(Group.m_aName), "Game");
		Writer.AddItem(MAPITEMTYPE_GROUP, 0, sizeof(Group), &Group);

		Writer.Finish();

		dbg_assert(m_Map.Load(m_pStorage.get(), m_TestInfo.m_aFilename, IStorage::TYPE_ALL), "load test map");
		m_Layers.Init(&m_Map, false, false);
		m_Collision.Init(&m_Layers);
	}

	~CAsciiWorld()
	{
		m_pStorage->RemoveFile(m_TestInfo.m_aFilename, IStorage::TYPE_SAVE);
	}

	CAsciiWorld(const CAsciiWorld &) = delete;
	CAsciiWorld &operator=(const CAsciiWorld &) = delete;

	CCollision *Collision() { return &m_Collision; }

private:
	CTestInfo m_TestInfo;
	std::unique_ptr<IStorage> m_pStorage;
	CMap m_Map;
	CLayers m_Layers;
	CCollision m_Collision;
};

// Walls on both sides, floor on rows 6-7, ceiling on row 0.
// Interior air spans world x in [32, 288) and y in [32, 192).
inline CAsciiWorld Room()
{
	return CAsciiWorld({
		"##########",
		"#........#",
		"#........#",
		"#........#",
		"#........#",
		"#........#",
		"##########",
		"##########",
	});
}

#endif // TEST_ASCII_WORLD_H
