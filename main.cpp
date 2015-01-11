#include <windows.h>

#include <cassert>
#include <ctime>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include <bitset>
#include <queue>

#include "tiledata.hpp"
using namespace std;

const unsigned int INF = (unsigned int)-1;
const unsigned int InvalidIndex = (unsigned int)-1;
const unsigned int MaxTileCount = 200;
const unsigned int MaxDoorCount = 4;		//a room may has as many doors as MaxDoorCount
const unsigned int MaxTileWidth = 18;
const unsigned int MaxTileHeight = 18;
const unsigned int MaxCorridorLength = 5;

enum GridType{
	GridUnused = 0,
	GridWall,
	GridFloor,
	GridDoor,
	GridTypeCount,
};

char GridChar[GridTypeCount] = {
	'_', 'x', '.', 'd',
};

enum DoorDirection{
	DoorWrong = -1,
	DoorUp = 0,
	DoorDown,
	DoorLeft,
	DoorRight,
	DoorDirectionCount,
};

enum LocateMode{
	Rotate0 = 0, // clockwise rotate
	Rotate90,
	Rotate180,
	Rotate270,
	HoriMirror,	// <--->
	VertMirror, // 
	LocateModeCount,
};

class Random{
	time_t m_seed;
public:
	Random() : m_seed(time(NULL)) {}
	unsigned int GetRand(unsigned int min, unsigned int max)
	{
		time_t seed;
		seed = time(NULL) + m_seed;
		m_seed = seed;

		srand((unsigned int)seed);

		unsigned int n = max - min + 1;
		unsigned int i = rand() % n;
		return min + i;
	}
};

struct Vector2{
	int x, y; // horizontal, vertical coordination
	Vector2(int _x = 0, int _y = 0) : x(_x), y(_y) { }
	void Set(int _x, int _y) { x = _x; y = _y; }
	inline bool operator==(const Vector2 &v) const {
		return x == v.x && y == v.y;
	}
	inline bool operator!=(const Vector2 &v) const {
		return operator==(v);
	}
	inline friend Vector2 operator+(const Vector2 &l, const Vector2 &r) {
		return Vector2(l.x + r.x, l.y + r.y);
	}
	inline friend Vector2 operator-(const Vector2 &l, const Vector2 &r) {
		return Vector2(l.x - r.x, l.y - r.y);
	}
	inline friend Vector2 operator-(const Vector2 &v) {
		return Vector2(v.x, v.y);
	}
	inline friend Vector2 operator*(const Vector2 &v, int len) {
		return Vector2(v.x * len, v.y * len);
	}
};

struct Rect{
	int x, y, h, w;
	Rect(int _x = 0, int _y = 0, int _h = 0, int _w = 0) : x(_x), y(_y), h(_h), w(_w) { }
	void Set(int _x, int _y, int _h, int _w) { x = _x; y = _y; h = _h; w = _w; }
	inline bool Intersect(const Rect &rect) const {
		if(x + h - 1 < rect.x || y + w - 1 < rect.y || rect.x + rect.h - 1 < x || rect.y + rect.w - 1 < y)
			return false;
		return true;
	}
	inline bool Contain(const Vector2 &p) const {
		if(p.x >= x && p.x <= x + h && p.y >= y && p.y <= y + w)
			return true;
		return false;
	}
};

struct Line{
	Vector2 start, end;
};

typedef std::vector<Line> LineVec;

struct Door{
	Vector2 locate;
	DoorDirection direction;
};

typedef std::vector<Door> DoorVec;

class Tile{
	unsigned int m_type_id;
	unsigned int m_width, m_height;
	char m_grids[MaxTileHeight][MaxTileWidth];
	DoorVec m_doors;
private:
	DoorDirection GetDoorDirection(unsigned int x, unsigned int y);
public:
	Tile(const char *grids[], unsigned int id);
	friend class Graph;
};

typedef std::vector<Tile*> TileVec;

DoorDirection Tile::GetDoorDirection(unsigned int x, unsigned int y)
{
	DoorDirection dir = DoorWrong;
	if(x == 0 && y > 0 && y < m_width - 1)
		dir = DoorUp;
	else if(x == m_height - 1 && y > 0 && y < m_width - 1)
		dir = DoorDown;
	else if(y == 0 && x > 0 && x < m_height - 1)
		dir = DoorLeft;
	else if(y == m_width - 1 && x > 0 && x < m_height - 1)
		dir = DoorRight;
	return dir;
}

Tile::Tile(const char *grids[], unsigned int id) 
	: m_width(0), m_height(0), m_type_id(id)
{
	unsigned int w = strlen(grids[0]);
	unsigned int h = 0;
	for (h = 0; h < MaxTileHeight; ++h) {
		if (strlen(grids[h]) == 0) {
			break;
		}
	}

	assert(w <= MaxTileWidth && h <= MaxTileHeight);

	m_width = w;
	m_height = h;

	for (unsigned int i = 0; i < h; ++i) {
		for (unsigned int j = 0; j < w; ++j) {
			m_grids[i][j] = grids[i][j];
			if (grids[i][j] == 'd') {
				DoorDirection dir = GetDoorDirection(i, j);
				if (dir != DoorWrong) {
					Door door;
					door.locate.Set(i, j);
					door.direction = dir;
					m_doors.push_back(door);
					assert(m_doors.size() <= MaxDoorCount);
				}
			}
		}
	}
}

struct Arrange{
	const Tile *m_tile;
	Vector2 m_pivot;
	Rect m_rect;
	LocateMode m_locate;
	Arrange() : m_tile(NULL), m_pivot(0, 0)
		, m_rect(0, 0, 0, 0), m_locate(Rotate0){ }
};

typedef std::vector<Arrange*> ArrangeVec;
typedef std::vector<unsigned int> IndexVec;

class Graph{
	ArrangeVec m_arranges;
	IndexVec m_adj_list[MaxTileCount];
	TileVec m_src_tiles[MaxDoorCount];
	DoorVec m_open_doors;
	LineVec m_lines;
	Random m_random;
	unsigned int m_max_door;
	unsigned int m_cur_tile_count;
private:
	void AddTile(Tile *tile);
	unsigned int FindArrange(const Vector2 &coord);
	Rect GetTileRect(const Tile *tile, const Vector2 &coord, LocateMode loca_mode) const;
	bool CheckTile(const Tile *tile, const Vector2 &coord, LocateMode loca_mode) const;
	void LinkTile(unsigned int arr_idx, const Tile *tile, const Vector2 &coord, LocateMode loca_mode);
	void AddDoors(const Tile *tile, const Vector2 &coord, LocateMode loca_mode, unsigned int exclude_door_idx = InvalidIndex);
	void DelDoor(unsigned int idx);
	void AddLink(const Vector2 &start, const Vector2 &end);
	Tile* ChooseEndTile();
	Tile* ChooseLinkTile();
	void GenNewLocate(const Door *src_door, const Door *dst_door, Vector2 &door_pos, Vector2 &coord, LocateMode &loca_mode);
	Vector2 TransformVector(LocateMode location, const Vector2 &v) const;
public:
	Graph();
	~Graph();
	void Reset();
	bool RandomGen(unsigned int tile_count);
	bool GenExact(unsigned int tile_count, unsigned int max_try = 100); // we try as many as "max_try" times to get a result with exactly has "tile_count" tiles
	void FindPath(unsigned int start_idx, unsigned int end_idx, IndexVec &path);
	void Print();
};

Graph::Graph() : m_max_door(0), m_cur_tile_count(0)
{
	Tile *tile = new Tile(tile1_0, 10);
	AddTile(tile);
	tile = new Tile(tile2_0, 20);
	AddTile(tile);
	tile = new Tile(tile2_1, 21);
	AddTile(tile);
	tile = new Tile(tile3_0, 30);
	AddTile(tile);
	tile = new Tile(tile3_1, 31);
	AddTile(tile);

	// check valid
	assert(m_max_door >= 2);
	for (unsigned int i = 0; i < m_max_door; ++i) {
		assert(m_src_tiles[i].size() > 0);
	}
}

Graph::~Graph()
{
	for (unsigned int i = 0; i < MaxDoorCount; ++i) {
		TileVec::iterator itr = m_src_tiles[i].begin();
		for (; itr != m_src_tiles[i].end(); ++itr) {
			delete (*itr);
		}
		m_src_tiles[i].clear();
	}
	
	ArrangeVec::iterator itr = m_arranges.begin();
	for (; itr != m_arranges.end(); ++itr) {
		delete (*itr);
	}
	m_arranges.clear();
	m_open_doors.clear();
}

void Graph::AddTile(Tile *tile)
{
	unsigned int door_count = tile->m_doors.size();
	assert(door_count > 0 && door_count <= MaxDoorCount);
	m_src_tiles[door_count - 1].push_back(tile);
	if (door_count > m_max_door) {
		m_max_door = door_count;
	}
}

void Graph::Reset()
{
	 m_cur_tile_count = 0;
	 ArrangeVec::iterator itr = m_arranges.begin();
	 for (; itr != m_arranges.end(); ++itr) {
		 delete (*itr);
	 }
	 m_arranges.clear();
	 m_open_doors.clear();
	 m_lines.clear();
	 
	 for (unsigned int i = 0; i < MaxTileCount; ++i) {
		m_adj_list[i].clear();
	 }
}

unsigned int Graph::FindArrange(const Vector2 &coord)
{
	for (unsigned int i = 0; i < m_arranges.size(); ++i) {
		if (m_arranges[i]->m_rect.Contain(coord)) {
			return i;
		}
	}
	return -1;
}

Rect Graph::GetTileRect(const Tile *tile, const Vector2 &coord, LocateMode loca_mode) const
{
	unsigned int h = 0, w = 0;
	unsigned int x = 0, y = 0;
	switch(loca_mode)
	{
	case Rotate0:
		x = coord.x;
		y = coord.y;
		h = tile->m_height;
		w = tile->m_width;
		break;
	case Rotate90:
		x = coord.x;
		y = coord.y - (tile->m_height - 1);
		h = tile->m_width;
		w = tile->m_height;
		break;
	case Rotate180:
		x = coord.x - (tile->m_height - 1);
		y = coord.y - (tile->m_width - 1);
		h = tile->m_height;
		w = tile->m_width;
		break;
	case Rotate270:
		x = coord.x - (tile->m_width - 1);
		y = coord.y;
		h = tile->m_width;
		w = tile->m_height;
		break;
	case HoriMirror:
		x = coord.x;
		y = coord.y - (tile->m_width - 1);
		h = tile->m_height;
		w = tile->m_width;
		break;
	case VertMirror:
		x = coord.x - (tile->m_height - 1);
		y = coord.y;
		h = tile->m_height;
		w = tile->m_width;
		break;
	default:
		break;
	}

	return Rect(x, y, h, w);
}

bool Graph::CheckTile(const Tile *tile, const Vector2 &coord, LocateMode loca_mode) const
{
	Rect rect = GetTileRect(tile, coord, loca_mode);
	for (unsigned int i = 0; i < m_arranges.size(); ++i) {
		if (rect.Intersect(m_arranges[i]->m_rect)) {
			return false;
		}
	}
	
	return true;
}

// the origin of a Tile locate at the CENTER point of topleft
void Graph::LinkTile(unsigned int arr_idx, const Tile *tile, const Vector2 &coord, LocateMode loca_mode)
{
	++m_cur_tile_count;
	Arrange *arrange = new Arrange;
	arrange->m_pivot = coord;
	arrange->m_rect = GetTileRect(tile, coord, loca_mode);
	arrange->m_tile = tile;
	arrange->m_locate = loca_mode;
	m_arranges.push_back(arrange);

	if (arr_idx != InvalidIndex) {
		// add to adjacency list
		unsigned int new_vertex = m_arranges.size() - 1;
		m_adj_list[arr_idx].push_back(new_vertex);
		m_adj_list[new_vertex].push_back(arr_idx);
	}
}

void Graph::AddDoors(const Tile *tile, const Vector2 &coord, LocateMode loca_mode, unsigned int exclude_door_idx)
{
	static DoorDirection door_dirs[LocateModeCount][DoorDirectionCount];
	door_dirs[Rotate0][DoorDown] = DoorDown;
	door_dirs[Rotate0][DoorUp] = DoorUp;
	door_dirs[Rotate0][DoorLeft] = DoorLeft;
	door_dirs[Rotate0][DoorRight] = DoorRight;
	door_dirs[Rotate90][DoorDown] = DoorLeft;
	door_dirs[Rotate90][DoorUp] = DoorRight;
	door_dirs[Rotate90][DoorLeft] = DoorUp;
	door_dirs[Rotate90][DoorRight] = DoorDown;
	door_dirs[Rotate180][DoorDown] = DoorUp;
	door_dirs[Rotate180][DoorUp] = DoorDown;
	door_dirs[Rotate180][DoorLeft] = DoorRight;
	door_dirs[Rotate180][DoorRight] = DoorLeft;
	door_dirs[Rotate270][DoorDown] = DoorRight;
	door_dirs[Rotate270][DoorUp] = DoorLeft;
	door_dirs[Rotate270][DoorLeft] = DoorDown;
	door_dirs[Rotate270][DoorRight] = DoorUp;
	door_dirs[HoriMirror][DoorDown] = DoorDown;
	door_dirs[HoriMirror][DoorUp] = DoorUp;
	door_dirs[HoriMirror][DoorLeft] = DoorRight;
	door_dirs[HoriMirror][DoorRight] = DoorLeft;
	door_dirs[VertMirror][DoorDown] = DoorUp;
	door_dirs[VertMirror][DoorUp] = DoorDown;
	door_dirs[VertMirror][DoorLeft] = DoorLeft;
	door_dirs[VertMirror][DoorRight] = DoorRight;
	for (unsigned int i = 0; i < tile->m_doors.size(); ++i) {
		if (i == exclude_door_idx) {
			continue;
		}
		Door door;
		door.direction = door_dirs[loca_mode][tile->m_doors[i].direction];
		door.locate = coord + TransformVector(loca_mode, tile->m_doors[i].locate);
		m_open_doors.push_back(door);
	}
}

void Graph::DelDoor(unsigned int idx)
{
	if (idx != -1) {
		m_open_doors[idx] = *m_open_doors.rbegin();
		m_open_doors.pop_back();
	}
}

void Graph::AddLink(const Vector2 &start, const Vector2 &end)
{
	Line line;
	line.start = start;
	line.end = end;
	m_lines.push_back(line);
}

Tile* Graph::ChooseEndTile()
{
	unsigned int tile_cnt = m_src_tiles[0].size();
	unsigned int tile_idx = m_random.GetRand(0, tile_cnt - 1);
	return m_src_tiles[0][tile_idx];
}

Tile* Graph::ChooseLinkTile()
{
	unsigned int index = m_random.GetRand(1, m_max_door - 1);
	unsigned int tile_cnt = m_src_tiles[index].size();
	unsigned int tile_idx = m_random.GetRand(0, tile_cnt - 1);
	return m_src_tiles[index][tile_idx];
}

void Graph::GenNewLocate(const Door *src_door, const Door *dst_door, Vector2 &door_pos, Vector2 &coord, LocateMode &loca_mode)
{
	static LocateMode localModes[DoorDirectionCount][DoorDirectionCount][2];
	localModes[DoorDown][DoorDown][0] = Rotate180;
	localModes[DoorDown][DoorDown][1] = VertMirror;
	localModes[DoorDown][DoorUp][0] = Rotate0;
	localModes[DoorDown][DoorUp][1] = HoriMirror;
	localModes[DoorDown][DoorLeft][0] = Rotate90;
	localModes[DoorDown][DoorLeft][1] = Rotate90;
	localModes[DoorDown][DoorRight][0] = Rotate270;
	localModes[DoorDown][DoorRight][1] = Rotate270;

	localModes[DoorUp][DoorDown][0] = Rotate0;
	localModes[DoorUp][DoorDown][1] = HoriMirror;
	localModes[DoorUp][DoorUp][0] = Rotate180;
	localModes[DoorUp][DoorUp][1] = VertMirror;
	localModes[DoorUp][DoorLeft][0] = Rotate270;
	localModes[DoorUp][DoorLeft][1] = Rotate270;
	localModes[DoorUp][DoorRight][0] = Rotate90;
	localModes[DoorUp][DoorRight][1] = Rotate90;

	localModes[DoorLeft][DoorDown][0] = Rotate270;
	localModes[DoorLeft][DoorDown][1] = Rotate270;
	localModes[DoorLeft][DoorUp][0] = Rotate90;
	localModes[DoorLeft][DoorUp][1] = Rotate90;
	localModes[DoorLeft][DoorLeft][0] = Rotate180;
	localModes[DoorLeft][DoorLeft][1] = HoriMirror;
	localModes[DoorLeft][DoorRight][0] = Rotate0;
	localModes[DoorLeft][DoorRight][1] = VertMirror;

	localModes[DoorRight][DoorDown][0] = Rotate90;
	localModes[DoorRight][DoorDown][1] = Rotate90;
	localModes[DoorRight][DoorUp][0] = Rotate270;
	localModes[DoorRight][DoorUp][1] = Rotate270;
	localModes[DoorRight][DoorLeft][0] = Rotate0;
	localModes[DoorRight][DoorLeft][1] = VertMirror;
	localModes[DoorRight][DoorRight][0] = Rotate180;
	localModes[DoorRight][DoorRight][1] = HoriMirror;

	int len = m_random.GetRand(1, MaxCorridorLength);
	Vector2 extends[DoorDirectionCount];
	extends[DoorDown].Set(1, 0);
	extends[DoorUp].Set(-1, 0);
	extends[DoorLeft].Set(0, -1);
	extends[DoorRight].Set(0, 1);
	door_pos = src_door->locate + extends[src_door->direction] * len;

	coord.Set(0, 0);
	loca_mode = localModes[src_door->direction][dst_door->direction][m_random.GetRand(0, 1)];
	coord = door_pos - TransformVector(loca_mode, dst_door->locate);
}

Vector2 Graph::TransformVector(LocateMode location, const Vector2 &v) const
{
	Vector2 vec(0, 0);
	switch(location)
	{
	case Rotate0:
		vec.Set(v.x, v.y);
		break;
	case Rotate90:
		vec.Set(v.y, -v.x);
		break;
	case Rotate180:
		vec.Set(-v.x, -v.y);
		break;
	case Rotate270:
		vec.Set(-v.y, v.x);
		break;
	case HoriMirror:
		vec.Set(v.x, -v.y);
		break;
	case VertMirror:
		vec.Set(-v.x, v.y);
		break;
	default:
		assert(0);
		break;
	}
	return vec;
}

bool Graph::RandomGen(unsigned int tile_count)
{
	assert(tile_count > 1 && tile_count <= MaxTileCount);
	Vector2 coord(MaxTileCount * MaxTileHeight/2, MaxTileCount * MaxTileWidth/2);
	LocateMode loca_mode = (LocateMode)m_random.GetRand(Rotate0, LocateModeCount - 1);
	loca_mode = Rotate0;
	Tile *tile = ChooseLinkTile();
	// the first tile as the root
	AddDoors(tile, coord, loca_mode, InvalidIndex);
	LinkTile(InvalidIndex, tile, coord, loca_mode);

	unsigned int arr_idx = 0;
	Vector2 door_pos(0, 0);
	// random link last (tile_count - 1) tile
	for(unsigned int i = 1; i < tile_count; ++i) {
		unsigned int src_door_idx = m_random.GetRand(0, m_open_doors.size() - 1);	
		arr_idx = FindArrange(m_open_doors[src_door_idx].locate);
		tile = ChooseLinkTile();
		unsigned int dst_door_idx = m_random.GetRand(0, tile->m_doors.size() - 1);
		GenNewLocate(&m_open_doors[src_door_idx], &tile->m_doors[dst_door_idx], door_pos, coord, loca_mode);
		if (CheckTile(tile, coord, loca_mode)) {
			AddLink(m_open_doors[src_door_idx].locate, door_pos);
			DelDoor(src_door_idx);
			AddDoors(tile, coord, loca_mode, dst_door_idx);
			LinkTile(arr_idx, tile, coord, loca_mode);
		}
		else{
			bool linked = false;
			for (unsigned int d = 0; d < m_open_doors.size(); ++d) {
				for (unsigned int k = 0; k < tile->m_doors.size(); ++k) {
					arr_idx = FindArrange(m_open_doors[d].locate);
					GenNewLocate(&m_open_doors[d], &tile->m_doors[k], door_pos, coord, loca_mode);
					if (CheckTile(tile, coord, loca_mode)) {
						linked = true;
						AddLink(m_open_doors[d].locate, door_pos);
						DelDoor(d);
						AddDoors(tile, coord, loca_mode, k);
						LinkTile(arr_idx, tile, coord, loca_mode);
						break;
					}
				}
				if (linked) {
					break;
				}
			}
		}
	}

	// link end tile to the door or just close it
	for (unsigned int i = 0; i < m_open_doors.size(); ++i) {
		arr_idx = FindArrange(m_open_doors[i].locate);
		tile = ChooseEndTile();
		GenNewLocate(&m_open_doors[i], &tile->m_doors[0], door_pos, coord, loca_mode);
		bool ok = CheckTile(tile, coord, loca_mode);
		if (ok && m_cur_tile_count < tile_count) {
			AddLink(m_open_doors[i].locate, door_pos);
			DelDoor(i);
			LinkTile(arr_idx, tile, coord, loca_mode);
		}
	}

	return m_cur_tile_count == tile_count;

}

bool Graph::GenExact(unsigned int tile_count, unsigned int max_try)
{
	bool ok = false;
	for (unsigned int i = 0; i < max_try && (!ok); ++i) {
		Reset();
		ok |= RandomGen(tile_count);
	}
	return ok;
}

void Graph::FindPath(unsigned int start_idx, unsigned int end_idx, IndexVec &path)
{
	unsigned int d[MaxTileCount]; // d[i] 源节点到节点i的最短距离
	unsigned int p[MaxTileCount]; // p[i]: 从源节点到节点i的最短路径上，节点i的前一节点
	bitset<MaxTileCount> vset;

	const unsigned int nv = m_arranges.size();
	queue<unsigned int> Q;

	unsigned int u_idx, v_idx;
	for(unsigned int i = 0; i< nv; ++i) {
		d[i] = INF;
		p[i] = -1;
	}
	d[start_idx] = 0;
	vset.reset();
	while(!Q.empty()) {
		Q.pop();
	}
	Q.push(start_idx);
	while(!Q.empty()) {
		u_idx = Q.front();
		Q.pop();
		for(unsigned int i = 0; i < m_adj_list[u_idx].size(); ++i) {
			v_idx = m_adj_list[u_idx][i];
			if(d[u_idx] != INF && d[v_idx] > d[u_idx] + 1) {
				d[v_idx] = d[u_idx] + 1;
				p[v_idx] = u_idx;
				if(!vset[v_idx]) {
					vset.set(v_idx);
					Q.push(v_idx);
				}
			}
		}
	}

	path.clear();
	unsigned int idx = end_idx;
	for (; idx != start_idx; idx = p[idx]) {
		path.push_back(idx);
	}
	path.push_back(start_idx);
	unsigned int l = 0, r = path.size() - 1;
	for (; l < r; ++l, --r) {
		std::swap<unsigned int>(path[l], path[r]);
	}
}

void Graph::Print()
{
	unsigned int h = MaxTileCount * MaxTileHeight;
	unsigned int w = MaxTileCount * MaxTileWidth;
	int left = w, right = 0, top = h, bottom = 0;
	for (unsigned int i = 0; i < m_arranges.size(); ++i) {
		Rect rect = m_arranges[i]->m_rect;
		top = std::min<unsigned int>(top, rect.x);
		bottom = std::max<unsigned int>(bottom, rect.x + rect.h);
		left = std::min<unsigned int>(left, rect.y);
		right = std::max<unsigned int>(right, rect.y + rect.w);
	}

	h = bottom - top + 1;
	w = right - left + 1;
	char **grids = new char*[h];
	for (unsigned int i = 0; i < h; ++i) {
		grids[i] = new char[w];
		memset(grids[i], GridChar[GridUnused], sizeof(char) * w);
	}

	for (unsigned int a = 0; a < m_arranges.size(); ++a) {
		Arrange arrange = *m_arranges[a];
		unsigned int nw = arrange.m_tile->m_width;
		unsigned int nh = arrange.m_tile->m_height;
		for (unsigned int i = 0; i < nh; ++i) {
			for (unsigned int j = 0; j < nw; ++j) {
				unsigned int x = 0, y = 0;
				unsigned int u = 0, v = 0;
				switch(arrange.m_locate)
				{
				case Rotate0:
					x = arrange.m_rect.x + i - top;
					y = arrange.m_rect.y + j - left;
					u = i;
					v = j;	
					break;
				case Rotate90:
					x = arrange.m_rect.x + j - top;
					y = arrange.m_rect.y + i - left;
					u = nh - i - 1;
					v = j;
					break;
				case Rotate180:
					x = arrange.m_rect.x + i - top;
					y = arrange.m_rect.y + j - left;
					u = nh - i - 1;
					v = nw - j - 1;
					break;
				case Rotate270:
					x = arrange.m_rect.x + j - top;
					y = arrange.m_rect.y + i - left;
					u = i;
					v = nw - j - 1;
					break;
				case HoriMirror:
					x = arrange.m_rect.x + i - top;
					y = arrange.m_rect.y + j - left;
					u = i;
					v = nw - j - 1;
					break;
				case VertMirror:
					x = arrange.m_rect.x + i - top;
					y = arrange.m_rect.y + j - left;
					u = nh - i - 1;
					v = j;
					break;
				default:
					break;
				}

				if(i + j == 0) grids[x][y] = 'A' + a;
				else grids[x][y] = arrange.m_tile->m_grids[u][v];
			}
		}
	}

	for (unsigned int i = 0; i < m_lines.size(); ++i)
	{
		if (m_lines[i].start.x == m_lines[i].end.x)
		{
			int x = m_lines[i].start.x;
			int len = std::abs(m_lines[i].start.y - m_lines[i].end.y) + 1;
			int ymin = std::min<int>(m_lines[i].start.y, m_lines[i].end.y);
			for (int j = 0; j < len; ++j)
			{
				int y = ymin + j;
				grids[x - top][y - left] = 'd';
			}
		}
		else if (m_lines[i].start.y == m_lines[i].end.y)
		{
			int y = m_lines[i].start.y;
			int len = std::abs(m_lines[i].start.x - m_lines[i].end.x) + 1;
			int xmin = std::min<int>(m_lines[i].start.x, m_lines[i].end.x);
			for (int j = 0; j < len; ++j)
			{
				int x = xmin + j;
				grids[x - top][y - left] = 'd';
			}
		}
	}

	cout<<m_cur_tile_count<<endl;
	cout<<setw(5)<<" ";
	for (unsigned int i = 0; i < w; ++i) {
		char c = 'A' + (i % 26);
		cout<<c;
	}
	cout<<endl;
	for (unsigned int i = 0; i < h; ++i) {
		cout<<setw(5)<<i;
		for (unsigned int j = 0; j < w; ++j) {
			cout<<grids[i][j];
		}
		cout<<endl;
	}
	cout<<endl;

	cout<<"adjacent matrix:"<<endl;

	if (m_arranges.size() <= 24) {
		for (unsigned int i = 0; i < m_arranges.size(); ++i) {
			cout<<"Node "<<(char)('A'+ i)<<"->";
			for (unsigned int j = 0; j < m_adj_list[i].size(); ++j) {
				cout<<(char)('A'+ m_adj_list[i][j]);
				if (j + 1 < m_adj_list[i].size()) {
					cout<<"->";
				}
				else {
					cout<<endl;
				}
			}
		}
		cout<<endl;
	}
	else{
		for (unsigned int i = 0; i < m_arranges.size(); ++i) {
			cout<<"Node "<<i<<"->";
			for (unsigned int j = 0; j < m_adj_list[i].size(); ++j) {
				cout<<m_adj_list[i][j];
				if (j + 1 < m_adj_list[i].size()) {
					cout<<"->";
				}
				else {
					cout<<endl;
				}
			}
		}
		cout<<endl;
	}

}

int main()
{
	Graph graph;
	const int n = 10;
	const unsigned int node_count = 22;
	IndexVec path;

	LARGE_INTEGER temp;
	::QueryPerformanceFrequency(&temp);
	double reci_freq = (1000.0 / temp.QuadPart);
	LARGE_INTEGER start;
	::QueryPerformanceCounter(&start);

	int cnt = 0;
	for (int i = 0; i < n; ++i)	{
		bool ok = graph.GenExact(node_count);
		cnt += (ok ? 0 : 1);
		graph.FindPath(0, node_count-1, path);
	}

	LARGE_INTEGER end;
	::QueryPerformanceCounter(&end);
	float t = (float)((end.QuadPart - start.QuadPart) * reci_freq);

	cout<<"total time(ms): "<<t<<endl;
	cout<<"average time(ms): "<<t/n<<endl;

	cout<<"MaxCnt = "<<cnt<<endl;
	
	graph.Print();

	cout<<"Path of "<<'A'<<" => "<<(char)('A'+ node_count - 1)<<":"<<endl;
	for (unsigned int k = 0; k < path.size(); ++k) {
		char c = 'A'+ path[k];
		cout<<c;
		if (k + 1 < path.size()) {
			cout<<" -> ";
		}
	}
	cout<<endl;

	return 0;
}

