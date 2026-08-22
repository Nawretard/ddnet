#include <base/mem.h>

#include <engine/shared/snapshot.h>

#include <generated/protocol.h>

#include <game/gamecore.h>

#include <gtest/gtest.h>

TEST(Snapshot, CrcOneInt)
{
	CSnapshotBuilder Builder;
	Builder.Init();

	CNetObj_Flag Flag;
	Flag.m_X = 4;
	Flag.m_Y = 0;
	Flag.m_Team = 0;
	ASSERT_TRUE(Builder.NewItem(NETOBJTYPE_FLAG, 0, &Flag, sizeof(Flag)));

	CSnapshotBuffer Buffer;
	Builder.Finish(&Buffer);
	ASSERT_EQ(Buffer.AsSnapshot()->Crc(), 4);
}

TEST(Snapshot, CrcTwoInts)
{
	CSnapshotBuilder Builder;
	Builder.Init();

	CNetObj_Flag Flag;
	Flag.m_X = 1;
	Flag.m_Y = 1;
	Flag.m_Team = 0;
	ASSERT_TRUE(Builder.NewItem(NETOBJTYPE_FLAG, 0, &Flag, sizeof(Flag)));

	CSnapshotBuffer Buffer;
	Builder.Finish(&Buffer);
	ASSERT_EQ(Buffer.AsSnapshot()->Crc(), 2);
}

TEST(Snapshot, CrcBiggerInts)
{
	CSnapshotBuilder Builder;
	Builder.Init();

	CNetObj_Flag Flag;
	Flag.m_X = 99999999;
	Flag.m_Y = 1;
	Flag.m_Team = 1;
	ASSERT_TRUE(Builder.NewItem(NETOBJTYPE_FLAG, 0, &Flag, sizeof(Flag)));

	CSnapshotBuffer Buffer;
	Builder.Finish(&Buffer);
	ASSERT_EQ(Buffer.AsSnapshot()->Crc(), 100000001);
}

TEST(Snapshot, CrcOverflow)
{
	CSnapshotBuilder Builder;
	Builder.Init();

	CNetObj_Flag Flag;
	Flag.m_X = 0xFFFFFFFF;
	Flag.m_Y = 1;
	Flag.m_Team = 1;
	ASSERT_TRUE(Builder.NewItem(NETOBJTYPE_FLAG, 0, &Flag, sizeof(Flag)));

	CSnapshotBuffer Buffer;
	Builder.Finish(&Buffer);
	ASSERT_EQ(Buffer.AsSnapshot()->Crc(), 1);
}

TEST(Snapshot, StorageGet)
{
	CSnapshotStorage Storage;

	// `CSnapshotStorage` needs snapshots in increasing tick order.
	const char aData[8] = {0};
	Storage.Add(10, 1000, 1, aData, 0, nullptr);
	Storage.Add(20, 2000, 2, aData, 0, nullptr);
	Storage.Add(30, 3000, 3, aData, 0, nullptr);
	Storage.Add(40, 4000, 4, aData, 0, nullptr);

	int64_t Tagtime = -1;

	// Retrieve existing snapshots.
	EXPECT_EQ(Storage.Get(40, &Tagtime, nullptr, nullptr), 4);
	EXPECT_EQ(Tagtime, 4000);
	EXPECT_EQ(Storage.Get(10, &Tagtime, nullptr, nullptr), 1);
	EXPECT_EQ(Tagtime, 1000);
	EXPECT_EQ(Storage.Get(30, &Tagtime, nullptr, nullptr), 3);
	EXPECT_EQ(Tagtime, 3000);

	// Check non-existing snapshots in before, within and after the range.
	EXPECT_EQ(Storage.Get(50, nullptr, nullptr, nullptr), -1);
	EXPECT_EQ(Storage.Get(5, nullptr, nullptr, nullptr), -1);
	EXPECT_EQ(Storage.Get(25, nullptr, nullptr, nullptr), -1);
}

TEST(Snapshot, AnItemThatStopsShortReadsAsNotToldRatherThanAsZero)
{
	// DDNetCharacter grows a field at a time and is not size-validated, so a server
	// that predates one sends an item that stops before it. The reader fills what is
	// missing with the field's declared default -- which is why a field whose zero
	// means something must default to a value outside its own domain.
	CNetObjHandler Handler;
	int aTruncated[(sizeof(CNetObj_DDNetCharacter) / sizeof(int)) - 1] = {};
	CUnpacker Unpacker;
	Unpacker.Reset(aTruncated, sizeof(aTruncated));

	const CNetObj_DDNetCharacter *pRead = (const CNetObj_DDNetCharacter *)Handler.SecureUnpackObj(NETOBJTYPE_DDNETCHARACTER, &Unpacker);

	ASSERT_NE(pRead, nullptr);
	EXPECT_EQ(pRead->m_Gravity, GRAVITY_UNTOLD);
	EXPECT_FALSE(IsGravityPreset(pRead->m_Gravity));
}

TEST(Snapshot, OnlyAnExtendedObjectIsFreeToChangeWidth)
{
	CNetObjHandler Handler;

	// Why the gravity field could be added at all: an extended object is registered
	// with no static size, so neither end assumes a width and one that grew is read
	// by one that never heard of the field.
	EXPECT_EQ(Handler.GetObjSize(NETOBJTYPE_DDNETCHARACTER), 0);

	// CNetObj_PlayerInput is the opposite kind, and m_WantedGravity widened it: a
	// vanilla object whose width *both* ends register in the snapshot delta's
	// static-size table, so ours no longer matches an upstream build's. That is
	// inert for exactly one reason -- no snapshot ever carries an input. Widening
	// this again, or snapping one, reopens the question against every official
	// server, and silently: the delta would decode at the wrong offsets.
	EXPECT_EQ(Handler.GetObjSize(NETOBJTYPE_PLAYERINPUT), 11 * (int)sizeof(int));
}
