//#define NOPLACE

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <climits>
#include <cmath>
#include <concepts>
#include <cstdlib>
#include <format>
#include <iostream>
#include <memory>
#include <random>
#include <source_location>
#include <stdexcept>
#include <stdio.h>
#include <thread>
#include <vector>
#include <fstream>
#include <cstdint>
#include <cstring>

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#define NOMINMAX
	#define VC_EXTRALEAN
	#define NOSERVICE
	#define NOMCX
	#define NOIME
	#define NOSOUND
	#include <windows.h>
#else
	#include <unistd.h>
	#include <pthread.h>
	#include <sched.h>
	extern "C" int nice(int);
#endif

#define NO_OKECC_SYNTAX
#include "okecc.h"
#include <csignal>

typedef uint8_t		ChipID_t;
typedef int			Energy_t;

constexpr int MAX_CHIPS = 225;

//////////////////////////////////////////////////////////////////////////////
// 焼きなまし法

struct Pos {
	uint8_t x, y;
	bool operator<(const Pos& other) const {
		if (x != other.x) return x < other.x;
		return y < other.y;
	}
};

struct PathNode {
	UINT id;
	int dist_to_exit; // EXITまでの最大距離
};

// 座標が未決定、または論理的に削除されたことを示す定数
static constexpr ChipID_t POS_INVALID = 0xFF;

class CarnageSA {
public:
	CChipPool& pool;
	std::vector<std::vector<ChipID_t>> LinkList; // チップごとの接続リスト

	UINT GridWidth;
	UINT GridHeight;
	std::array<Pos, MAX_CHIPS> state;
	const char *m_name;
	Energy_t best_E;

	// 8方向のベクトル定義 (N, NE, E, SE, S, SW, W, NW)
	inline static const int dx[] = { 0,  1,  1,  1,  0, -1, -1, -1, 0};
	inline static const int dy[] = {-1, -1,  0,  1,  1,  1,  0, -1, 0};

	inline static constexpr int max_iter	= 2000000;
	inline static constexpr double StartT	= 2000.0;
	inline static constexpr double EndT		= 0.005;

	CarnageSA(CChipPool& p, const char *name = nullptr)
		: pool(p), GridWidth(p.m_width), GridHeight(p.m_height), m_name(name){
		MakeLinkList();
	}

	void InitState(void);
	int run(UINT num_threads = 0);
	void run_single(UINT uThreadID, double CurrentT);
	Energy_t calculate_energy(std::array<Pos, MAX_CHIPS>& state, const std::array<ChipID_t, MAX_CHIPS>& occ_org, std::vector<ChipID_t>& move_chip_list);
	void SetArrow(void);

	void NopRouting(void);
	void rebuild_occ(const std::array<Pos, MAX_CHIPS>& state, std::array<ChipID_t, MAX_CHIPS>& occ);
	void MakeLinkList();
	Energy_t FindPath(UINT from, UINT to, std::array<Pos, MAX_CHIPS>& state, std::array<ChipID_t, MAX_CHIPS>& occ, bool PlaceNop = false, bool rxg = false);

	const std::array<Pos, MAX_CHIPS>& get_result() const { return state; }

	void dump(void){
		printf("Start=%d\n x  y  ID   G   R  Description\n", pool.m_start);
		for(UINT u = 0; u < pool.size(); ++u){
			if(pool[u]){
				std::string s = pool[u]->GetLayoutText();
				std::replace(s.begin(), s.end(), '\n', ' ');

				Pos& p = state[u];

				printf("%2d %2d %3d %3d %3d  %s\n",
					p.x, p.y,
					u,
					pool[u]->m_NextG,
					pool[u]->m_NextR,
					s.c_str()
				);
			}
		}
	}

	enum{
		S_FOUND_ZERO,
		S_FOUND,
		S_NOT_FOUND,
	};

	inline static std::atomic<Energy_t> OverallScore = std::numeric_limits<Energy_t>::max();

	// 割り込みハンドラ，割込み時に OverallScore = -1 にする
	static void InterruptHandler(int signum){
		OverallScore.store(-1, std::memory_order_relaxed);
	}

	Energy_t UpdateScore(Energy_t score){
		Energy_t NewScore = score;
		Energy_t OtherScore = OverallScore.load(std::memory_order_relaxed);

		// 現在の共有ベストスコアよりも小さい場合のみ、更新を試みる
		while (NewScore < OtherScore) {
			if (OverallScore.compare_exchange_weak(
				OtherScore, NewScore,
				std::memory_order_release,
				std::memory_order_relaxed
			)){
				// 更新成功
				break;
			}
		}

		return std::min<Energy_t>(score, OtherScore);
	}
};

void CarnageSA::InitState() {
	state.fill({POS_INVALID, POS_INVALID});

	for (UINT u = 0; u < (UINT)pool.size(); ++u) {
		UINT y = u / GridWidth;
		UINT x = u % GridWidth;
		x = (y % 2 == 0) ? x : (GridWidth - 1 - x);
		state[u] = {(uint8_t)x, (uint8_t)y};

		// 接続リストの構築
		UINT v;
		if ((v = pool[u]->m_NextG) < pool.size()) {
			LinkList[u].push_back(v);
			LinkList[v].push_back(u); // 双方向に接続
		}
		if ((v = pool[u]->m_NextR) < pool.size()) {
			LinkList[u].push_back(v);
			LinkList[v].push_back(u); // 双方向に接続
		}
	}
}

void CarnageSA::MakeLinkList() {
	LinkList.resize(pool.size());

	for (UINT u = 0; u < (UINT)pool.size(); ++u) {
		// 接続リストの構築
		UINT v;
		if ((v = pool[u]->m_NextG) < pool.size()) {
			LinkList[u].push_back(v);
			LinkList[v].push_back(u); // 双方向に接続
		}
		if ((v = pool[u]->m_NextR) < pool.size()) {
			LinkList[u].push_back(v);
			LinkList[v].push_back(u); // 双方向に接続
		}
	}
}

inline Energy_t CarnageSA::FindPath(UINT from, UINT to, std::array<Pos, MAX_CHIPS>& state, std::array<ChipID_t, MAX_CHIPS>& occ, bool PlaceNop, bool rxg){
	CChip *chip;

	if(PlaceNop) chip = pool[from];

	auto IsBrank = [&](int x, int y) -> bool {
		return occ[y * GridWidth + x] == POS_INVALID;
	};

	auto Occupy = [&](int x, int y){
		if(PlaceNop){
			UINT nop = pool.add(std::make_unique<CChipNop>());

			if(rxg){
				chip->m_NextR = nop;
			}else{
				chip->m_NextG = nop;
			}

			rxg = 0;

			occ[y * GridWidth + x] = (ChipID_t)nop;
			chip = pool[nop];
			state[nop] = {(uint8_t)x, (uint8_t)y};
		}else{
			occ[y * GridWidth + x] = 0;
		}
	};

	// チップ path が他のチップを通過
	int stx = state[from].x;
	int sty = state[from].y;
	int edx = state[to].x;
	int edy = state[to].y;

	if(
		PlaceNop &&
		std::max(std::abs(stx - edx), std::abs(sty - edy)) == 1
	) return 0;

	if(std::abs(stx - edx) >= std::abs(sty - edy)){
		int stepx = stx < edx ? 1 : -1;
		int y = sty;
		int dx = std::abs(edx - stx) - 1;

		for(int x = stx + stepx; x != edx; x += stepx){
			if(y == edy){
				if(IsBrank(x, y)){
					Occupy(x, y);
				}else if(y >= 1 && IsBrank(x, y - 1)){
					Occupy(x, y - 1);
					--y;
				}else if(y < (int)GridHeight - 1 && IsBrank(x, y + 1)){
					Occupy(x, y + 1);
					++y;
				}else{
					return 1;
				}
			}

			else if(y > edy){
				if(y >= 1 && IsBrank(x, y - 1)){
					Occupy(x, y - 1);
					--y;
				}else if(std::abs(y - edy) <= dx && IsBrank(x, y)){
					Occupy(x, y);
				}else if(std::abs(y - edy) <  dx && y < (int)GridHeight - 1 && IsBrank(x, y + 1)){
					Occupy(x, y + 1);
					++y;
				}else{
					return 1;
				}
			}

			else{
				if(y < (int)GridHeight - 1 && IsBrank(x, y + 1)){
					Occupy(x, y + 1);
					++y;
				}else if(std::abs(y - edy) <= dx && IsBrank(x, y)){
					Occupy(x, y);
				}else if(std::abs(y - edy) <  dx && y >= 1 && IsBrank(x, y - 1)){
					Occupy(x, y - 1);
					--y;
				}else{
					return 1;
				}
			}

			--dx;
		}
	}else{
		int stepy = sty < edy ? 1 : -1;
		int x = stx;
		int dy = std::abs(edy - sty) - 1;

		for(int y = sty + stepy; y != edy; y += stepy){
			if(x == edx){
				if(IsBrank(x, y)){
					Occupy(x, y);
				}else if(x >= 1 && IsBrank(x - 1, y)){
					Occupy(x - 1, y);
					--x;
				}else if(x < (int)GridWidth - 1 && IsBrank(x + 1, y)){
					Occupy(x + 1, y);
					++x;
				}else{
					return 1;
				}
			}

			else if(x > edx){
				if(x >= 1 && IsBrank(x - 1, y)){
					Occupy(x - 1, y);
					--x;
				}else if(std::abs(x - edx) <= dy && IsBrank(x, y)){
					Occupy(x, y);
				}else if(std::abs(x - edx) <  dy && x < (int)GridWidth - 1 && IsBrank(x + 1, y)){
					Occupy(x + 1, y);
					++x;
				}else{
					return 1;
				}
			}

			else{
				if(x < (int)GridWidth - 1 && IsBrank(x + 1, y)){
					Occupy(x + 1, y);
					++x;
				}else if(std::abs(x - edx) <= dy && IsBrank(x, y)){
					Occupy(x, y);
				}else if(std::abs(x - edx) <  dy && x >= 1 && IsBrank(x - 1, y)){
					Occupy(x - 1, y);
					--x;
				}else{
					return 1;
				}
			}

			--dy;
		}
	}

	if(PlaceNop){
		chip->m_NextG = to;
	}

	return 0;
}

Energy_t CarnageSA::calculate_energy(
	std::array<Pos, MAX_CHIPS>& state,
	 const std::array<ChipID_t,
	 MAX_CHIPS>& occ_org,
	 std::vector<ChipID_t>& move_chip_list
){
	// ペナルティ
	const Energy_t distance_penalty		= 1;	// chip 距離
	const Energy_t overwrap_penalty		= 100;	// チップ間 path が他のチップを通過
	const Energy_t start_end_penalty	= 1000;	// start/end 枠からの距離

	const size_t N = pool.size();

	auto occ = occ_org;

	// 優先移動リスト
	Energy_t base_energy = 0;
	move_chip_list.resize(0);
	std::array<uint8_t, MAX_CHIPS> Added = {};

	// パラメータ（調整可能）

	for (UINT u = 0; u < (UINT)pool.size(); ++u) {
		Pos p = state[u];
		CChip* chip = pool[u];
		Energy_t chip_energy = 0;

		// 物理制約
		if (!(p.x < GridWidth) || !(p.y < GridHeight)) chip_energy += 5000;

		// Startチップ制約
		if (u == pool.m_start && p.y != 0) chip_energy += p.y * start_end_penalty;

		// exit チップ制約
		if(pool[u]->m_NextG == IDX_EXIT || pool[u]->m_NextR == IDX_EXIT){
			chip_energy += start_end_penalty * (std::min)(
				std::min<int>(p.x, GridWidth - p.x - 1),
				std::min<int>(p.y, GridHeight- p.y - 1)
			);
		}

		auto ChipE = [&](UINT ToIdx) -> Energy_t {
			// チップ距離
			Pos to = state[ToIdx];

			auto e = distance_penalty * ((std::max)(
				std::abs((int)p.x - (int)to.x), std::abs((int)p.y - (int)to.y)
			) - 1);

			if(e == 0) return e;
			return e + FindPath(u, ToIdx, state, occ) * overwrap_penalty;
		};

		if(pool[u]->m_NextG < N) chip_energy += ChipE(pool[u]->m_NextG);
		if(pool[u]->m_NextR < N) chip_energy += ChipE(pool[u]->m_NextR);

		// 優先移動リストに登録
		auto AddMoveList = [&](UINT u){
			if(u < N && !Added[u]){
				move_chip_list.push_back(u);
				Added[u] = 1;
			}
		};

		if(chip_energy > 0){
			AddMoveList(u);
			AddMoveList(pool[u]->m_NextG);
			AddMoveList(pool[u]->m_NextR);
		}

		base_energy += chip_energy;
	}

	if (base_energy <= 0) return 0;
	return base_energy;
}

void CarnageSA::rebuild_occ(const std::array<Pos, MAX_CHIPS>& state, std::array<ChipID_t, MAX_CHIPS>& occ){
	std::fill(occ.begin(), occ.end(), POS_INVALID);

	for (UINT j = 0; j < (UINT)pool.size(); ++j) {
		const Pos& pp = state[j];
		if (pp.x < GridWidth && pp.y < GridHeight){
			occ[pp.y * GridWidth + pp.x] = (ChipID_t)j;
		}
	}
};

void CarnageSA::run_single(UINT uThreadID, double CurrentT) {

	const double Tcooling	= std::pow(EndT / CurrentT, 1.0 / max_iter);

	constexpr UINT p_random_swap	= 1;	// ランダムスワップ
	constexpr UINT p_nearby_swap	= 5;	// 隣接スワップ
	constexpr UINT p_move_mid		= 50;	// 接続chip の真ん中に移動

	std::mt19937_64 gen;
	gen.seed(std::random_device{}());
	std::uniform_real_distribution<double> dist_prob(0.0, 1.0);

	auto GetRand = [&gen](uint32_t max) -> uint32_t {
		return (static_cast<uint64_t>(static_cast<uint32_t>(gen())) * max) >> 32;
	};

	// occupancy
	std::array<ChipID_t, MAX_CHIPS> occ;
	std::array<ChipID_t, MAX_CHIPS> next_occ;

	// 優先移動対象リスト
	using param_t = std::uniform_int_distribution<UINT>::param_type;
	std::vector<ChipID_t> move_chip_list;
	move_chip_list.reserve(MAX_CHIPS);

	// ログ表示タイマ
	using clock = std::chrono::steady_clock;
	auto next_log_time = clock::now();  // 最初の起点
	const auto interval = std::chrono::seconds(1);

#ifdef _WIN32
	HANDLE hThread = GetCurrentThread();
	SetThreadPriority(hThread, THREAD_PRIORITY_BELOW_NORMAL);
#else
	// Linux: nice値を下げる（優先度低）
	nice(10);
#endif

	std::array<Pos, MAX_CHIPS> best_state;
	rebuild_occ(state, occ);

	auto dump_occ = [&](std::array<ChipID_t, MAX_CHIPS>& occ){
		printf("----------------\n");
		for(UINT u = 0; u < GridWidth * GridHeight; ++u){
			if(occ[u] == POS_INVALID) printf("-- ");
			else printf("%02d ", occ[u]);
			if(u % GridWidth == (GridWidth - 1)) printf("\n");
		}
	};

	Energy_t current_E	= calculate_energy(state, occ, move_chip_list);

	best_state	= state;
	best_E		= current_E;

	// move probabilities (base)
	// neighbor-swap が残りの確率
	auto next_state = state;

	// 1chip 移動
	auto SwapChipXY = [&](UINT ax, UINT ay, UINT bx, UINT by) {
		UINT achip = next_occ[ay * GridWidth + ax];
		UINT bchip = next_occ[by * GridWidth + bx];

		std::swap(next_occ[ay * GridWidth + ax], next_occ[by * GridWidth + bx]);

		if(achip != POS_INVALID){
			next_state[achip].x = (uint8_t)bx;
			next_state[achip].y = (uint8_t)by;
		}
		if(bchip != POS_INVALID){
			next_state[bchip].x = (uint8_t)ax;
			next_state[bchip].y = (uint8_t)ay;
		}
	};

	int iter;
	for (iter = 0; iter < max_iter; ++iter) {
		next_state	= state;
		next_occ	= occ;

		UINT move_strategy = GetRand(
			p_random_swap +
			p_nearby_swap +
			p_move_mid
		);

		bool proposed = false;

		if (move_strategy < p_random_swap) {
			UINT SrcIdx = GetRand((uint32_t)pool.size());

			// ランダムセルへのジャンプ（空きがあれば移動、なければスワップ）
			int nx = GetRand(GridWidth);
			int ny = GetRand(GridHeight);

			SwapChipXY(next_state[SrcIdx].x, next_state[SrcIdx].y, nx, ny);
			proposed = true;
		}
		else {
			// 移動元選択
			UINT SrcIdx;

			if(GetRand(10) == 0){
				SrcIdx = GetRand((uint32_t)pool.size());
			}else{
				// 優先リストから選ぶ
				SrcIdx = GetRand((uint32_t)move_chip_list.size());
				int x = next_state[SrcIdx].x;
				int y = next_state[SrcIdx].y;

				// 優先チップの 5x5 周辺からランダムに選ぶ
				while(1){
					int pos_rand = GetRand(25);
					x += (pos_rand % 5) - 2;
					if(x < 0) x = 0;
					else if(x >= (int)GridWidth) x = GridWidth - 1;

					y += (pos_rand / 5) - 2;
					if(y < 0) y = 0;
					else if(y >= (int)GridHeight) y = GridHeight - 1;

					if((SrcIdx = next_occ[y * GridWidth + x]) != POS_INVALID) break;
				}
			}

			if(move_strategy < p_random_swap + p_nearby_swap){
				// 隣接セルとのスワップ（8方向）
				int dir = GetRand(8);
				UINT nx = next_state[SrcIdx].x + dx[dir];
				UINT ny = next_state[SrcIdx].y + dy[dir];
				if (nx < GridWidth && ny < GridHeight){
					SwapChipXY(next_state[SrcIdx].x, next_state[SrcIdx].y, nx, ny);
					proposed = true;
				}
			}
			else {
				// 接続チップの真ん中に移動
				if (LinkList[SrcIdx].empty()) continue; // ないはず

				UINT dst_x = 0, dst_y = 0;
				for(UINT neighbor : LinkList[SrcIdx]){
					dst_x += next_state[neighbor].x;
					dst_y += next_state[neighbor].y;
				}

				UINT n = (UINT)LinkList[SrcIdx].size();
				dst_x = (dst_x + n / 2) / n;
				dst_y = (dst_y + n / 2) / n;

				// 隣接セルとのスワップ（8方向）
				int dir = GetRand(9);
				dst_x += dx[dir];
				dst_y += dy[dir];

				if(dst_x >= GridWidth ){dst_x -= dx[dir];}
				if(dst_y >= GridHeight){dst_y -= dy[dir];}

				auto& p = next_state[SrcIdx];

				// 移動先が同じなら continue
				if (p.x == dst_x && p.y == dst_y){
					--iter;
					continue;
				}

				// start チップの場合は dst_y=0 に固定
				if (SrcIdx == pool.m_start) dst_y = 0;

				// Exit チップの場合は端に固定
				else if(pool[SrcIdx]->m_NextG == IDX_EXIT || pool[SrcIdx]->m_NextR == IDX_EXIT){
					UINT xdist = (std::min)(dst_x, GridWidth  - 1 - dst_x);
					UINT ydist = (std::min)(dst_y, GridHeight - 1 - dst_y);

					if (xdist < ydist) {
						dst_x = (dst_x < GridWidth / 2) ? 0 : (GridWidth - 1);
					} else {
						dst_y = (dst_y < GridHeight / 2) ? 0 : (GridHeight - 1);
					}
				}

				// 移動先が空きならそのまま移動
				if (next_occ[dst_y * GridWidth + dst_x] == POS_INVALID) {
					SwapChipXY(next_state[SrcIdx].x, next_state[SrcIdx].y, dst_x, dst_y);
					proposed = true;
				}else{
					// 一旦 chip を消す
					next_occ[p.y * GridWidth + p.x] = POS_INVALID;

					// 最も近い空きセルを探す
					UINT best_x = 0;
					UINT best_y = 0;
					UINT best_dist = INT_MAX;

					for(UINT y = 0; y < GridHeight; ++y){
						for(UINT x = 0; x < GridWidth; ++x){
							int occIdx = next_occ[y * GridWidth + x];
							if (occIdx == POS_INVALID) {
								UINT dist = std::abs((int)(x - dst_x)) + std::abs((int)(y - dst_y));
								if (dist < best_dist) {
									best_dist = dist;
									best_x = x;
									best_y = y;
								}
							}
						}
					}

					// チップ移動
					if(best_x < dst_x){
						for (UINT x = best_x; x < dst_x; ++x) SwapChipXY(x + 1, best_y, x, best_y);
					}else{
						for (UINT x = best_x; x > dst_x; --x) SwapChipXY(x - 1, best_y, x, best_y);
					}
					if(best_y < dst_y){
						for (UINT y = best_y; y < dst_y; ++y) SwapChipXY(dst_x, y + 1, dst_x, y);
					}else{
						for (UINT y = best_y; y > dst_y; --y) SwapChipXY(dst_x, y - 1, dst_x, y);
					}

					next_occ[dst_y * GridWidth + dst_x] = (ChipID_t)SrcIdx;
					next_state[SrcIdx].x = (uint8_t)dst_x;
					next_state[SrcIdx].y = (uint8_t)dst_y;
					proposed = true;
				}
			}
		}

		if (!proposed){
			--iter;
			continue;
		}

		Energy_t next_E = calculate_energy(next_state, next_occ, move_chip_list);

		// 早期最適性判定
		if (next_E == 0) {
			state		= next_state;
			current_E	= next_E;
			best_state	= next_state;
			best_E		= next_E;
			break;
		}

		int diff = (int)next_E - (int)current_E;
		if (diff < 0 || dist_prob(gen) < std::exp(-diff / CurrentT)) {
			// accept
			state		= next_state;
			occ			= next_occ;
			current_E	= next_E;
			if (current_E <= best_E) {
				best_E		= current_E;
				best_state	= state;
			}
		}

		Energy_t BestScore = UpdateScore(best_E);

		if (BestScore <= 0) break;

		// Tcooling
		CurrentT *= Tcooling;

		#ifndef DEBUG
			if (uThreadID == 0 && iter % 100000 == 0){
				auto now = clock::now();
				if (now >= next_log_time) {
					next_log_time = now + interval;
					printf("Step:%8d | T: %7.2f Score:%6d | Best:%6d\n", iter, CurrentT, (UINT)current_E, (UINT)BestScore);
				}
			}
		#endif
	}

	Energy_t BestScore = UpdateScore(best_E);

	if(uThreadID == 0) printf("Step:%8d | T: %7.2f Score:%6d | Best:%6d\n", iter, CurrentT, (UINT)current_E, (UINT)BestScore);
	state = best_state;
}

int CarnageSA::run(UINT num_threads){
	std::vector<CarnageSA> workers;
	std::vector<std::thread> threads;

	auto BestState = state;
	Energy_t BestScore = std::numeric_limits<Energy_t>::max();
	Energy_t PrevScore = std::numeric_limits<Energy_t>::max();

	#if defined _DEBUG || defined NOPLACE
		num_threads = 1;
	#else
		if(num_threads == 0) num_threads = std::thread::hardware_concurrency();
	#endif

	workers.reserve(num_threads);
	threads.reserve(num_threads);

	auto start = std::chrono::steady_clock::now();

	// スレッドごとに T を変える
	const double Tfactor = num_threads == 1 ? 1.0 : std::pow(0.0001, 1.0 / (num_threads - 1));

	// 割込みハンドラの設定
	std::signal(SIGINT, InterruptHandler);

	// 最初から探索やり直し
	bool bRestart = true;

	for(UINT uLoopCnt = 0; ; ++uLoopCnt){
		workers.clear();
		threads.clear();

		double T = StartT;

		if(bRestart){
			OverallScore = std::numeric_limits<Energy_t>::max();
			InitState();
		}
		
		#ifdef NOPLACE
			break;
		#endif
		
		for (UINT u = 0; u < num_threads; ++u) {
			// 1. スレッドごとに自分(*this)をコピーして独立したインスタンスを作る
			workers.push_back(*this);

			// 2. 各インスタンスの run を並列実行
			threads.emplace_back(&CarnageSA::run_single, &workers[u], u, T);

			if (!bRestart) T *= Tfactor;
		}

		// 3. 終了待機
		for (auto& t : threads) {
			if (t.joinable()) t.join();
		}

		// 4. 最良の結果を自分自身(*this)に書き戻す
		auto best_it = std::min_element(workers.begin(), workers.end(),
			[](const CarnageSA& a, const CarnageSA& b) {
				return a.best_E < b.best_E;
			}
		);

		state = best_it->state;
		best_E = best_it->best_E;

		// 5. 全体の最良スコアを更新
		if(best_E >= 0 && best_E < BestScore){
			BestScore = best_E;
			BestState = state;
		}

		// スコアが良化すればステートを継続して探索、悪化すれば再初期化して探索
		if(best_E >= 0 && (best_E / 100) < (PrevScore / 100)){
			bRestart = false;
			PrevScore = best_E;
		}else{
			bRestart = true;
			PrevScore = std::numeric_limits<Energy_t>::max();
		}

		if(best_E == 0 || OverallScore.load(std::memory_order_relaxed) <= 0 || uLoopCnt >= 1 && best_E < 100){
			state = BestState;
			best_E = BestScore;

			NopRouting();
			SetArrow();
			break;
		}
	}

	auto end = std::chrono::steady_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	printf("Parallel run finished in %.2fs. Best score: %d\n", elapsed / 1000.0, (UINT)this->best_E);

	return OverallScore.load(std::memory_order_relaxed) < 0;
}

//////////////////////////////////////////////////////////////////////////////
// Nop ルーティング

void CarnageSA::NopRouting(void){

	std::array<ChipID_t, MAX_CHIPS> occ;
	rebuild_occ(state, occ);

	UINT N = (UINT)pool.size();

	for (UINT u = 0; u < N; ++u) {
		if(pool[u]->m_NextG < N) FindPath(u, pool[u]->m_NextG, state, occ, true, false);
		if(pool[u]->m_NextR < N) FindPath(u, pool[u]->m_NextR, state, occ, true, true);
	}
}

//////////////////////////////////////////////////////////////////////////////
// RawG/R 設定

void CarnageSA::SetArrow(void){

	auto GetArrowCode = [&](UINT from, UINT to) -> int {
		if(to == IDX_NONE) return 4;

		if(to == IDX_EXIT){
			// EXIT は外枠への矢印とする
			if     (state[from].y == (GridHeight - 1))	return 4; // S
			else if(state[from].x == 0)					return 6; // W
			else if(state[from].x == (GridWidth - 1))	return 2; // E
			else										return (pool.m_start == from) ? 7 : 0;
		}

		int dx = (int)state[to].x - (int)state[from].x;
		int dy = (int)state[to].y - (int)state[from].y;

		if(dy < 0){
			if(dx < 0)			return 7; // NW
			else if(dx == 0)	return 0; // N
			else				return 1; // NE
		}
		else if(dy == 0){
			if(dx < 0)			return 6; // W
			else				return 2; // E
		}
		if(dx < 0)				return 5; // SW
		else if(dx == 0)		return 4; // S
		else					return 3; // SE
	};

	for(UINT u = 0; u < pool.size(); ++u){
		CChip* chip = pool[u];
		chip->m_RawG = GetArrowCode(u, chip->m_NextG);
		chip->m_RawR = GetArrowCode(u, chip->m_NextR);
	}
}

//////////////////////////////////////////////////////////////////////////////

void OutputSvg(const char* filename, const std::vector<CarnageSA*>& sa_list) {
	const int CHIP_SIZE = 75;    // チップサイズ
	const int GAP = 15;          // 隙間
	const int CELL_SIZE = CHIP_SIZE + GAP;
	const int OFFSET = (CELL_SIZE - CHIP_SIZE) / 2;
	const int SECTION_GAP = 60;  // セクション間の余白（タイトル含む）

	FILE* fp = fopen(filename, "w");
	if (!fp) return;

	// 各 SA の有効な高さを計算するためのラムダ
	auto get_effective_height = [&](CarnageSA* sa) {
		int max_y = -1;
		const auto& state = sa->get_result();
		for (int i = 0; i < (int)sa->pool.size(); ++i) {
			if (sa->pool.m_list[i] != nullptr && state[i].x != 255) {
				if ((int)state[i].y > max_y) max_y = (int)state[i].y;
			}
		}
		return max_y + 1; // 0-indexed なので +1
		};

	// 全体の描画サイズを計算
	int max_width = 0;
	int total_height = 0;
	std::vector<int> effective_heights;
	for (auto sa : sa_list) {
		if (sa->pool.size() == 0) {
			effective_heights.push_back(0);
			continue;
		}
		int h = get_effective_height(sa);
		effective_heights.push_back(h);
		max_width = std::max(max_width, (int)sa->pool.m_width * CELL_SIZE);
		total_height += h * CELL_SIZE + SECTION_GAP;
	}

	#ifdef _DEBUG
		fprintf(fp, "<?xml version=\"1.0\" encoding=\"SJIS\"?>\n");
	#else
		fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	#endif
	fprintf(fp, "<svg width=\"%d\" height=\"%d\" viewBox=\"0 0 %d %d\" xmlns=\"http://www.w3.org/2000/svg\">\n",
		max_width, total_height + 20, max_width, total_height + 20);

	fprintf(fp, "  <defs>\n");
	fprintf(fp, "    <marker id=\"arrow\" markerWidth=\"10\" markerHeight=\"10\" refX=\"9\" refY=\"3\" orient=\"auto\" markerUnits=\"strokeWidth\">\n");
	fprintf(fp, "      <path d=\"M0,0 L0,6 L9,3 z\" fill=\"context-stroke\" />\n");
	fprintf(fp, "    </marker>\n");
	fprintf(fp, "  </defs>\n");

	int current_y_offset = 0;

	for (size_t sa_idx = 0; sa_idx < sa_list.size(); ++sa_idx) {
		CarnageSA* sa = sa_list[sa_idx];
		int h = effective_heights[sa_idx];
		if (sa->pool.size() == 0 || h <= 0) continue;

		const auto& pool = sa->pool;
		const auto& state_disp = sa->get_result();

		// 💡 エラーチップを判定する関数内関数（ラムダ式）
		auto is_error_chip = [&](int idx) -> bool {
			if (pool.m_list[idx] == nullptr || state_disp[idx].x == 255) return false;

			int cx = state_disp[idx].x;
			int cy = state_disp[idx].y;
			auto chip = pool.m_list[idx].get();

			// 条件1: Y=0 でない start チップ
			if ((UINT)idx == pool.m_start && cy != 0) {
				return true;
			}

			// 接続先(NextG, NextR)のチェック
			UINT nexts[2] = { chip->m_NextG, chip->m_NextR };
			for (UINT next_idx : nexts) {
				if (next_idx == IDX_NONE) continue;

				// 条件2: IDX_EXIT かつ grid 外周に接していない
				if (next_idx == IDX_EXIT) {
					bool on_edge = (cx == 0 || cx == (int)pool.m_width - 1 || cy == 0 || cy == h - 1);
					if (!on_edge) return true;
				}
				// 条件3: 接続先がタテ・ヨコ・ナナメに隣接していない
				else if (next_idx < sa->pool.size() && state_disp[next_idx].x != 255) {
					int nx = state_disp[next_idx].x;
					int ny = state_disp[next_idx].y;
					if (std::abs(cx - nx) > 1 || std::abs(cy - ny) > 1) {
						return true;
					}
				}
			}
			return false;
			};

		// タイトル表示
		fprintf(fp, "  <text x=\"0\" y=\"%d\" font-family=\"Meiryo, sans-serif\" font-size=\"18\" font-weight=\"bold\" fill=\"#333\">%s</text>\n",
			current_y_offset + 25, sa->m_name);

		int draw_y_start = current_y_offset + 40;

		auto draw_physical_edge = [&](double x1, double y1, double x2, double y2, const char* color, bool dashed = false) {
			double dx = x2 - x1;
			double dy = y2 - y1;
			double dist = sqrt(dx * dx + dy * dy);
			if (dist < 0.1) return;
			double margin = (CHIP_SIZE / 2.0) + 2;
			const char* dash = dashed ? " stroke-dasharray=\"4\"" : "";
			fprintf(fp, "  <line x1=\"%.1f\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\" stroke=\"%s\" stroke-width=\"2.5\"%s marker-end=\"url(#arrow)\" />\n",
				x1 + dx * (margin / dist), draw_y_start + y1 + dy * (margin / dist),
				x2 - dx * (margin / dist), draw_y_start + y2 - dy * (margin / dist), color, dash);
			};

		// STARTへの入力
		if (pool.m_start < (UINT)sa->pool.size() && state_disp[pool.m_start].x != 255) {
			double x = state_disp[pool.m_start].x * CELL_SIZE + CELL_SIZE / 2.0;
			double y = state_disp[pool.m_start].y * CELL_SIZE + CELL_SIZE / 2.0;
			draw_physical_edge(x, y - CELL_SIZE, x, y, "#28a745");
		}

		// チップ描画
		for (int i = 0; i < (int)sa->pool.size(); ++i) {
			if (pool.m_list[i] == nullptr || state_disp[i].x == 255) continue;
			int x = state_disp[i].x * CELL_SIZE + OFFSET;
			int y = state_disp[i].y * CELL_SIZE + OFFSET;
			int centerX = x + CHIP_SIZE / 2;

			const char* fill_color =
				pool.m_list[i]->m_Id.get() == CChip::CHIPID_NOP ? "#F0F0F0" :
				pool.m_list[i]->m_NextR != IDX_NONE ? "#FFF0F0" : "white";

			// 💡 エラーチェックを行い、枠線の色と太さを決定
			const char* stroke_color = "#343a40";
			int stroke_width = 2;
			if (is_error_chip(i)) {
				stroke_color = "#dc3545"; // エラーは赤色
				stroke_width = 4;         // エラーは太線
			}

			fprintf(fp, "  <rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" rx=\"4\" fill=\"%s\" stroke=\"%s\" stroke-width=\"%d\" />\n",
				x, draw_y_start + y, CHIP_SIZE, CHIP_SIZE, fill_color, stroke_color, stroke_width);
			fprintf(fp, "  <text x=\"%d\" y=\"%d\" font-family=\"Meiryo, sans-serif\" text-anchor=\"middle\">\n", centerX, draw_y_start + y + 22);
			std::string txt = pool.m_list[i]->GetLayoutText();
			if (txt.empty()) {
				fprintf(fp, "    <tspan x=\"%d\" dy=\"1.2em\" font-size=\"10\" fill=\"#6c757d\">ID:%d</tspan>\n", centerX, i);
			}
			else {
				size_t s = 0, e = 0; int line_count = 0;
				while ((e = txt.find('\n', s)) != std::string::npos) {
					fprintf(fp, "    <tspan x=\"%d\" dy=\"%s\" font-size=\"11\" fill=\"#333\">%s</tspan>\n",
						centerX, (line_count == 0 ? "0" : "1.2em"), txt.substr(s, e - s).c_str());
					s = e + 1; line_count++;
				}
				fprintf(fp, "    <tspan x=\"%d\" dy=\"%s\" font-size=\"11\" fill=\"#333\">%s</tspan>\n",
					centerX, (line_count == 0 ? "0" : "1.2em"), txt.substr(s).c_str());
				fprintf(fp, "    <tspan x=\"%d\" dy=\"1.5em\" font-size=\"9\" fill=\"#999\">#%d</tspan>\n", centerX, i);
			}
			fprintf(fp, "  </text>\n");
		}

		// 接続線描画
		for (int i = 0; i < (int)sa->pool.size(); ++i) {
			if (pool.m_list[i] == nullptr || state_disp[i].x == 255) continue;
			double x1 = state_disp[i].x * CELL_SIZE + CELL_SIZE / 2.0;
			double y1 = state_disp[i].y * CELL_SIZE + CELL_SIZE / 2.0;
			auto process_conn = [&](UINT next_idx, const char* color) {
				if (next_idx == IDX_NONE) return;
				if (next_idx == IDX_EXIT) {
					double tx = x1, ty = y1;
					if (state_disp[i].x == 0) tx -= CELL_SIZE;
					else if (state_disp[i].x == pool.m_width - 1) tx += CELL_SIZE;
					else if (state_disp[i].y == 0) ty -= CELL_SIZE;
					else ty += CELL_SIZE;
					draw_physical_edge(x1, y1, tx, ty, color, true);
					return;
				}
				if (next_idx < sa->pool.size() && state_disp[next_idx].x != 255) {
					draw_physical_edge(x1, y1, state_disp[next_idx].x * CELL_SIZE + CELL_SIZE / 2.0, state_disp[next_idx].y * CELL_SIZE + CELL_SIZE / 2.0, color);
				}
			};
			process_conn(pool.m_list[i]->m_NextG, "#28a745");
			process_conn(pool.m_list[i]->m_NextR, "#dc3545");
		}

		current_y_offset += h * CELL_SIZE + SECTION_GAP;
	}
	fprintf(fp, "</svg>\n");
	fclose(fp);
}

void OutputSvg(const char* filename, CarnageSA* sa) {
	std::vector<CarnageSA*> sa_list = { sa };
	OutputSvg(filename, sa_list);
}

//////////////////////////////////////////////////////////////////////////////

class OkeSoft {
public:
	constexpr static UINT MAX_CHIP = 15 * 15 + 7 * 7 * 2;
	constexpr static UINT CHIP_START[3] = { 0, 15 * 15, 15 * 15 + 7 * 7 };

	OkeSoft(uint32_t main_size){
		size[0] = main_size;
		size[1] = size[2] = main_size == 15 ? 7 : 0;
		for (int i = 0; i < MAX_CHIP; ++i) chip[i] = 0x4080;
	}

	uint64_t	chip[MAX_CHIP];
	uint32_t	size[3];
	uint32_t	start[3] = {0, 0, 0};
};

//////////////////////////////////////////////////////////////////////////////
// CPU サイズ

void setCpuSize(int size){
	if(g_pCurField != g_pField[0].get()){
		std::cout << "Error: CPU size can only be set in the MAIN field." << std::endl;
		exit(1);
	}

	if(size < 4 || size > 15){
		std::cout << "Error: CPU size must be between 4 and 15." << std::endl;
		exit(1);
	}

	g_pCurField->m_pool.m_width = size;
	g_pCurField->m_pool.m_height = size;
}

//////////////////////////////////////////////////////////////////////////////

void chip_main(void);

int main(void){
	g_pField.push_back(std::make_unique<CField>("MAIN", 15, 15));
	g_pField.push_back(std::make_unique<CField>("SUB1", 7, 7));
	g_pField.push_back(std::make_unique<CField>("SUB2", 7, 7));
	g_pCurField = g_pField[0].get();

	chip_main();
	g_pCurField->CheckBlockStack();
	if(g_uErrorCnt) exit(1);

	std::vector<std::unique_ptr<CarnageSA>>	sa;
	std::vector<CarnageSA*> sa_ptrs;

	int RunResult = 0;
	for(int i = 0; i < 3; ++i){
		//g_pField[i]->m_pool.dump();
		if (g_pField[i]->FinalizeCompile() < 0) return 1;

		sa.emplace_back(std::make_unique<CarnageSA>(g_pField[i]->m_pool, g_pField[i]->m_name));
		sa_ptrs.push_back(sa[i].get());

		if(g_pField[i]->m_pool.size()){
			RunResult |= sa[i]->run();
			//sa[i]->dump();
		}
	}

	OutputSvg("okecc.svg", sa_ptrs);
	if(RunResult){
		std::cout << "Error: Some chips are not connected properly." << std::endl;
		return 1;
	}

	OkeSoft soft(g_pField[0]->m_pool.m_width);

	bool bMcrUpdate = false;

	// ソフト書き込み
	for (UINT fld = 0; fld < 3; ++fld) {
		auto& state = sa[fld]->get_result();
		auto pFld = g_pField[fld].get();

		if (pFld->m_pool.size() == 0) continue;

		// スタート位置
		soft.start[fld] = state[pFld->m_pool.m_start].x;

		for (UINT u = 0; u < pFld->m_pool.size(); ++u) {
			CChipBinary bin;

			auto& p = state[u];
			pFld->m_pool[u]->GetBin(bin);
			soft.chip[soft.CHIP_START[fld] + p.y * soft.size[fld] + p.x] = bin.m_val;
		}

		bMcrUpdate = true;
	}

	// 読み込み・書き込み・バイナリモードで開く
	std::string filepath = "DATA.BIN";
	std::fstream file(filepath, std::ios::in | std::ios::out | std::ios::binary);

	if (!file) {
		std::cout << "Can't open file: " << filepath << "\n";
		return 1;
	}

	// 1. ファイルサイズを取得
	file.seekg(0, std::ios::end);
	std::streamsize size = file.tellg();
	if (size <= 0) {
		std::cout << "File is empty or size cannot be determined.	\n";
		return 1;
	}

	// 2. バッファに全データを読み込む
	file.seekg(0, std::ios::beg);
	std::vector<uint8_t> buffer(static_cast<size_t>(size));
	if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
		std::cout << "Failed to read file data.\n";
		return 1;
	}

	// 3. データの書き換え
	memcpy(buffer.data() + 0xFE6, &soft, sizeof(soft));

	// CPU サイズの書き換え
	int cpu_size = g_pField[0]->m_pool.m_width;
	buffer[0x2BE] =
		cpu_size == 4 ? 0 :
		cpu_size <= 6 ? 1 :
		cpu_size <= 10 ? 3 : 5;

	// 4. 書き込み位置をファイルの先頭に戻す
	file.seekp(0, std::ios::beg);

	// 5. 書き換えたバッファを上書き
	if (!file.write(reinterpret_cast<const char*>(buffer.data()), size)) {
		std::cout << "Failed to write updated data to file.\n";
		return 1;
	}

	std::cout << "Successfully updated " << filepath << ".\n";
	return 0;
}
