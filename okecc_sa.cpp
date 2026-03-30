#include <array>
#include <algorithm>
#include <cassert>
#include <climits>
#include <cmath>
#include <concepts>
#include <cstdlib>
#include <format>
#include <iostream>
#include <random>
#include <source_location>
#include <stdexcept>
#include <stdio.h>
#include <vector>
#include <thread>
#include <chrono>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define VC_EXTRALEAN
#define NOSERVICE
#define NOMCX
#define NOIME
#define NOSOUND
#include <windows.h>

#define NO_OKECC_SYNTAX
#include "okecc.h"
#include "mcmgr.h"

typedef uint8_t ChipID_t;

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

template <size_t MAX_CHIPS = 256>
class CarnageSA {
	CChipPool& pool;
	std::vector<std::vector<ChipID_t>> LinkList; // チップごとの接続リスト

	UINT GridWidth;
	UINT GridHeight;
	std::array<Pos, MAX_CHIPS> state;
	const char *m_svg_file;
	double best_E;

	// 8方向のベクトル定義 (N, NE, E, SE, S, SW, W, NW)
	inline static const int dx[] = { 0,  1,  1,  1,  0, -1, -1, -1, 0};
	inline static const int dy[] = {-1, -1,  0,  1,  1,  1,  0, -1, 0};
	
public:
	CarnageSA(CChipPool& p, const char *svg_file = nullptr)
		: pool(p), GridWidth(p.m_width), GridHeight(p.m_height), m_svg_file(svg_file){
		LinkList.resize(pool.size());
		state.fill({POS_INVALID, POS_INVALID});
		initialize();
	}
	
	// 座標の正規化：最小値を (0, 0) に合わせる
	void normalize_coordinates() {
		int min_x = INT_MAX, min_y = INT_MAX;
		bool has_data = false;
	
		for (UINT i = 0; i < (UINT)pool.size(); ++i) {
			const auto& p = state[i];
			if (p.x != POS_INVALID) {
				min_x = (std::min)(min_x, (int)p.x);
				min_y = (std::min)(min_y, (int)p.y);
				has_data = true;
			}
		}
	
		if (!has_data) return;
	
		for (UINT i = 0; i < (UINT)pool.size(); ++i) {
			auto& p = state[i];
			if (p.x != POS_INVALID) {
				p.x -= min_x;
				p.y -= min_y;
			}
		}
	}
	
	void initialize(void);
	void run(UINT num_threads = 0);
	void run_single();
	double calculate_energy(const std::array<Pos, MAX_CHIPS>& state, const std::array<ChipID_t, MAX_CHIPS>& occ_org);
	void SetArrow(void);
	void OutputSvg(const char* filename, const std::array<Pos, MAX_CHIPS>& state_disp);
	void NopRouting(void);
	void rebuild_occ(const std::array<Pos, MAX_CHIPS>& state, std::array<ChipID_t, MAX_CHIPS>& occ);
	
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
};

template <size_t MAX_CHIPS>
void CarnageSA<MAX_CHIPS>::initialize() {
	for (UINT i = 0; i < (UINT)pool.size(); ++i) {
		UINT y = i / GridWidth;
		UINT x = i % GridWidth;
		x = (y % 2 == 0) ? x : (GridWidth - 1 - x);
		state[i] = {(uint8_t)x, (uint8_t)y};
		
		// 接続リストの構築
		UINT u;
		if ((u = pool[i]->m_NextG) < pool.size()) {
			LinkList[i].push_back(u);
			LinkList[u].push_back(i); // 双方向に接続
		}
		if ((u = pool[i]->m_NextR) < pool.size()) {
			LinkList[i].push_back(u);
			LinkList[u].push_back(i); // 双方向に接続
		}
	}
	
	OutputSvg(m_svg_file, state);
}

template <size_t MAX_CHIPS>
double CarnageSA<MAX_CHIPS>::calculate_energy(const std::array<Pos, MAX_CHIPS>& state, const std::array<ChipID_t, MAX_CHIPS>& occ_org) {
	double base_energy = 0.0;
	
	// ペナルティ
	const double distance_penalty	= 1.0;		// chip 距離
	const double overwrap_penalty	= 100.0;	// チップ間 path が他のチップを通過
	const double start_end_penalty	= 1000.0;	// start/end 枠からの距離
	
	const size_t N = pool.size();
	
	auto occ = occ_org;

	// パラメータ（調整可能）

	for (UINT u = 0; u < (UINT)pool.size(); ++u) {
		Pos p = state[u];
		CChip* chip = pool[u];

		// 物理制約
		if (!(p.x < GridWidth) || !(p.y < GridHeight)) base_energy += 5000.0;

		// Startチップ制約
		if (u == pool.m_start && p.y != 0) base_energy += p.y * start_end_penalty;
		
		// exit チップ制約
		if(pool[u]->m_NextG == IDX_EXIT || pool[u]->m_NextR == IDX_EXIT){
			base_energy += start_end_penalty * (std::min)(
				std::min<int>(p.x, GridWidth - p.x - 1),
				std::min<int>(p.y, GridHeight- p.y - 1)
			);
		}
		
		auto IsBrank = [&](int x, int y) -> bool {
			return occ[y * GridWidth + x] == POS_INVALID;
		};
		
		auto Occupy = [&](int x, int y){
			occ[y * GridWidth + x] = 0;
		};
		
		auto ChipE = [&](UINT ToIdx) -> double {
			// チップ距離
			Pos to = state[ToIdx];
			
			auto e = distance_penalty * ((std::max)(
				std::abs((int)p.x - (int)to.x), std::abs((int)p.y - (int)to.y)
			) - 1);
			
			if(e == 0) return e;
			
			// チップ path が他のチップを通過
			int stx = p.x;
			int sty = p.y;
			int edx = state[ToIdx].x;
			int edy = state[ToIdx].y;
			
			if(std::abs(stx - edx) >= std::abs(sty - edy)){
				int stepx = stx < edx ? 1 : -1;
				
				for(int x = stx + stepx; x != edx; x += stepx){
					auto [y, mod] = std::div((edy - sty) * (x - stx) * stepx, (edx - stx) * stepx);
					y += sty;
					
					if(IsBrank(x, y)){
						Occupy(x, y);
					}else if(mod < 0 && IsBrank(x, y - 1)){
						Occupy(x, y - 1);
					}else if(mod > 0 && IsBrank(x, y + 1)){
						Occupy(x, y + 1);
					}else{
						e += overwrap_penalty;
					}
				}
			}else{
				int stepy = sty < edy ? 1 : -1;
				
				for(int y = sty + stepy; y != edy; y += stepy){
					auto [x, mod] = std::div((edx - stx) * (y - sty) * stepy, (edy - sty) * stepy);
					x += stx;
					
					if(IsBrank(x, y)){
						Occupy(x, y);
					}else if(mod < 0 && IsBrank(x - 1, y)){
						Occupy(x - 1, y);
					}else if(mod > 0 && IsBrank(x + 1, y)){
						Occupy(x + 1, y);
					}else{
						e += overwrap_penalty;
					}
				}
			}
			return e;
		};
		
		if(pool[u]->m_NextG < N) base_energy += ChipE(pool[u]->m_NextG);
		if(pool[u]->m_NextR < N) base_energy += ChipE(pool[u]->m_NextR);
	}

	if (base_energy <= 0.0) return 0.0;
	return base_energy;
}

template <size_t MAX_CHIPS>
void CarnageSA<MAX_CHIPS>::rebuild_occ(const std::array<Pos, MAX_CHIPS>& state, std::array<ChipID_t, MAX_CHIPS>& occ){
	std::fill(occ.begin(), occ.end(), POS_INVALID);
	
	for (UINT j = 0; j < (UINT)pool.size(); ++j) {
		const Pos& pp = state[j];
		if (pp.x < GridWidth && pp.y < GridHeight){
			occ[pp.y * GridWidth + pp.x] = (ChipID_t)j;
		}
	}
};

template <size_t MAX_CHIPS>
void CarnageSA<MAX_CHIPS>::run_single() {
	constexpr int max_iter	= 20000000;
	double T				= 2000.0;
	constexpr double EndT	= 0.05;
	const double cooling	= std::pow(EndT / T, 1.0 / max_iter);
	
	constexpr UINT p_random_swap	= 1;	// ランダムスワップ
	constexpr UINT p_nearby_swap	= 5;	// 隣接スワップ
	constexpr UINT p_move_mid		= 50;	// 接続chip の真ん中に移動
	
	std::mt19937_64 gen;
	gen.seed(std::random_device{}());
	
	std::uniform_int_distribution<UINT>		dist_idx(0, (UINT)pool.size() - 1);
	std::uniform_int_distribution<int>		dist_dir8(0, 7);
	std::uniform_int_distribution<int>		dist_dir9(0, 8);
	std::uniform_int_distribution<UINT>		dist_prob(0,
		p_random_swap +
		p_nearby_swap +
		p_move_mid
	);
	std::uniform_int_distribution<int> dist_x(0, GridWidth - 1);
	std::uniform_int_distribution<int> dist_y(0, GridHeight - 1);

	// occupancy
	std::array<ChipID_t, MAX_CHIPS> occ;
	std::array<ChipID_t, MAX_CHIPS> next_occ;
	
	HANDLE hThread = GetCurrentThread();
	if (!SetThreadPriority(hThread, THREAD_PRIORITY_BELOW_NORMAL)) {
		// 失敗した場合の処理（通常は無視しても動作に支障はありません）
	}
	
	rebuild_occ(state, occ);
	
	auto dump_occ = [&](std::array<ChipID_t, MAX_CHIPS>& occ){
		printf("----------------\n");
		for(UINT u = 0; u < GridWidth * GridHeight; ++u){
			if(occ[u] == POS_INVALID) printf("-- ");
			else printf("%02d ", occ[u]);
			if(u % GridWidth == (GridWidth - 1)) printf("\n");
		}
	};
	
	double current_E	= calculate_energy(state, occ);
	auto best_state		= state;
	best_E				= current_E;
	bool UpdateBest		= false;

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
		
		UINT SrcIdx = dist_idx(gen);
		UINT rnd = dist_prob(gen);
		
		bool proposed = false;

		if (rnd < p_random_swap) {
			// ランダムセルへのジャンプ（空きがあれば移動、なければスワップ）
			int nx = dist_x(gen);
			int ny = dist_y(gen);
			
			SwapChipXY(next_state[SrcIdx].x, next_state[SrcIdx].y, nx, ny);
			proposed = true;
		}
		else if(rnd < p_random_swap + p_nearby_swap){
			// 隣接セルとのスワップ（8方向）
			int dir = dist_dir8(gen);
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
			int dir = dist_dir9(gen);
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

		if (!proposed){
			--iter;
			continue;
		}

		double next_E = calculate_energy(next_state, next_occ);
		
		// 早期最適性判定
		if (next_E < 0.0001) {
			state		= next_state;
			current_E	= next_E;
			best_state	= next_state;
			best_E		= next_E;
			
			UpdateBest	= true;
			break;
		}

		double diff = next_E - current_E;
		if (diff < 0 || dist_prob(gen) < std::exp(-diff / T)) {
			// accept
			state		= next_state;
			occ			= next_occ;
			current_E	= next_E;
			if (current_E < best_E - 1e-9) {
				best_E		= current_E;
				best_state	= state;
				UpdateBest	= true;
			}
		}

		// cooling
		T *= cooling;
		
		#ifdef DEBUG
			if (iter % 500000 == 0){
				//dump_occ(next_occ);
				printf("Step: %7d | T: %7.2f Energy: %8.2f | Best: %8.2f\n", iter, T, current_E, best_E);
				OutputSvg("z.svg", next_state);
				if(UpdateBest){
					OutputSvg(m_svg_file, state);
					UpdateBest = false;
				}
			}
		#endif
	}

	state = best_state;
	
	#ifdef DEBUG
		printf("Step: %7d | Energy: %.2f\n", iter, best_E);
	#endif
}

template <size_t MAX_CHIPS>
void CarnageSA<MAX_CHIPS>::run(UINT num_threads){
	std::vector<CarnageSA<MAX_CHIPS>> workers;
	std::vector<std::thread> threads;
	if(num_threads == 0) num_threads = std::thread::hardware_concurrency();
	
	auto start = std::chrono::steady_clock::now();
	
	// 1. スレッドごとに自分(*this)をコピーして独立したインスタンスを作る
	for (UINT u = 0; u < num_threads; ++u) {
		workers.push_back(*this); 
	}

	// 2. 各インスタンスの run を並列実行
	
	for (UINT u = 0; u < num_threads; ++u) {
		threads.emplace_back(&CarnageSA<MAX_CHIPS>::run_single, &workers[u]);
	}

	// 3. 終了待機
	for (auto& t : threads) {
		if (t.joinable()) t.join();
	}

	// 4. 最良の結果を自分自身(*this)に書き戻す
	auto best_it = std::min_element(workers.begin(), workers.end(), 
		[](const CarnageSA<MAX_CHIPS>& a, const CarnageSA<MAX_CHIPS>& b) {
			return a.best_E < b.best_E;
		});

	this->state = best_it->state;
	this->best_E = best_it->best_E;
	
	auto end = std::chrono::steady_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	
	if(best_E < 100){
		NopRouting();
	}
	
	std::cout << "Parallel run finished in " << elapsed << "ms. Best energy: " << this->best_E << std::endl;
}

//////////////////////////////////////////////////////////////////////////////
// Nop ルーティング

template <size_t MAX_CHIPS>
void CarnageSA<MAX_CHIPS>::NopRouting(void){
	
	std::array<ChipID_t, MAX_CHIPS> occ;
	rebuild_occ(state, occ);
	
	UINT N = (UINT)pool.size();
	
	for (UINT u = 0; u < N; ++u) {
		Pos p = state[u];
		CChip* chip = pool[u];
		
		auto PlaceNop = [&](UINT ToIdx, bool rxg){
			
			auto IsBrank = [&](int x, int y) -> bool {
				return occ[y * GridWidth + x] == POS_INVALID;
			};
			
			auto Occupy = [&](int x, int y){
				UINT nop = pool.add(new CChipNop());
				
				if(rxg){
					chip->m_NextR = nop;
				}else{
					chip->m_NextG = nop;
				}
				
				occ[y * GridWidth + x] = (ChipID_t)nop;
				chip = pool[nop];
				state[nop] = {(uint8_t)x, (uint8_t)y};
			};
			
			// チップ距離
			Pos to = state[ToIdx];
			
			if((std::max)(std::abs((int)p.x - (int)to.x), std::abs((int)p.y - (int)to.y)) == 1) return;
			
			// チップ path が他のチップを通過
			int stx = p.x;
			int sty = p.y;
			int edx = state[ToIdx].x;
			int edy = state[ToIdx].y;
			
			if(std::abs(stx - edx) >= std::abs(sty - edy)){
				int stepx = stx < edx ? 1 : -1;
				
				for(int x = stx + stepx; x != edx; x += stepx){
					auto [y, mod] = std::div((edy - sty) * (x - stx) * stepx, (edx - stx) * stepx);
					y += sty;
					
					if(IsBrank(x, y)){
						Occupy(x, y);
					}else if(mod < 0 && IsBrank(x, y - 1)){
						Occupy(x, y - 1);
					}else if(mod > 0 && IsBrank(x, y + 1)){
						Occupy(x, y + 1);
					}
				}
			}else{
				int stepy = sty < edy ? 1 : -1;
				
				for(int y = sty + stepy; y != edy; y += stepy){
					auto [x, mod] = std::div((edx - stx) * (y - sty) * stepy, (edy - sty) * stepy);
					x += stx;
					
					if(IsBrank(x, y)){
						Occupy(x, y);
					}else if(mod < 0 && IsBrank(x - 1, y)){
						Occupy(x - 1, y);
					}else if(mod > 0 && IsBrank(x + 1, y)){
						Occupy(x + 1, y);
					}
				}
			}
			
			chip->m_NextG = ToIdx;
		};
		
		if(pool[u]->m_NextG < N) PlaceNop(pool[u]->m_NextG, 0);
		chip = pool[u];
		if(pool[u]->m_NextR < N) PlaceNop(pool[u]->m_NextR, 1);
	}
}

//////////////////////////////////////////////////////////////////////////////
// RawG/R 設定

template <size_t MAX_CHIPS>
void CarnageSA<MAX_CHIPS>::SetArrow(void){
	
	auto GetArrowCode = [&](UINT from, UINT to) -> int {
		if(to == IDX_NONE) return 0;

		if(to == IDX_EXIT){
			// EXIT は外枠への矢印とする
			if(state[from].y == 0)						return 0; // N
			else if(state[from].y == (GridHeight - 1))	return 4; // S
			else if(state[from].x == 0)					return 6; // W
			else if(state[from].x == (GridWidth - 1))	return 2; // E
			else										return 0; // 内部にある場合はとりあえず N
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

template <size_t MAX_CHIPS>
void CarnageSA<MAX_CHIPS>::OutputSvg(const char* filename, const std::array<Pos, MAX_CHIPS>& state_disp) {
	const int CHIP_SIZE = 75;    // チップサイズ
	const int GAP = 15;          // 隙間
	const int CELL_SIZE = CHIP_SIZE + GAP; 
	const int OFFSET = (CELL_SIZE - CHIP_SIZE) / 2;

	FILE* fp = fopen(filename, "w");
	if (!fp) return;

	// 1. 描画範囲の計算
	int min_x = 0;
	int min_y = 0;
	int max_x = pool.m_width - 1;
	int max_y = pool.m_height - 1;

	int vb_width = (max_x - min_x + 1) * CELL_SIZE + 100;
	int vb_height = (max_y - min_y + 1) * CELL_SIZE + 100;
	int vb_x = min_x * CELL_SIZE - 50;
	int vb_y = min_y * CELL_SIZE - 50;

	fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	fprintf(fp, "<svg width=\"%d\" height=\"%d\" viewBox=\"%d %d %d %d\" xmlns=\"http://www.w3.org/2000/svg\">\n", 
			vb_width, vb_height, vb_x, vb_y, vb_width, vb_height);
	
	// マーカー定義
	fprintf(fp, "  <defs>\n");
	fprintf(fp, "    <marker id=\"arrow\" markerWidth=\"10\" markerHeight=\"10\" refX=\"9\" refY=\"3\" orient=\"auto\" markerUnits=\"strokeWidth\">\n");
	fprintf(fp, "      <path d=\"M0,0 L0,6 L9,3 z\" fill=\"context-stroke\" />\n");
	fprintf(fp, "    </marker>\n");
	fprintf(fp, "  </defs>\n");

	// 背景
	fprintf(fp, "  <rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" fill=\"#f8f9fa\" />\n", vb_x, vb_y, vb_width, vb_height);

	// 2. 接続線描画用ラムダ（チップの縁で止まる計算を共通化）
	auto draw_physical_edge = [&](double x1, double y1, double x2, double y2, const char* color, bool dashed = false) {
		double dx = x2 - x1;
		double dy = y2 - y1;
		double dist = sqrt(dx*dx + dy*dy);
		if (dist < 0.1) return;

		double margin = (CHIP_SIZE / 2.0) + 2;
		const char* dash = dashed ? " stroke-dasharray=\"4\"" : "";

		fprintf(fp, "  <line x1=\"%.1f\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\" stroke=\"%s\" stroke-width=\"2.5\"%s marker-end=\"url(#arrow)\" />\n",
				x1 + dx * (margin/dist), y1 + dy * (margin/dist), 
				x2 - dx * (margin/dist), y2 - dy * (margin/dist), color, dash);
	};

	// 3. 特殊エッジ：STARTへの入力
	if (pool.m_start < (UINT)pool.size() && state_disp[pool.m_start].x != POS_INVALID) {
		double x = state_disp[pool.m_start].x * CELL_SIZE + CELL_SIZE / 2.0;
		double y = state_disp[pool.m_start].y * CELL_SIZE + CELL_SIZE / 2.0;
		// 1マス上にある仮想チップからの接続として描画
		draw_physical_edge(x, y - CELL_SIZE, x, y, "#28a745");
	}

	// 4. チップ本体の描画
	for (int i = 0; i < (int)pool.size(); ++i) {
		if (pool.m_list[i] == nullptr || state_disp[i].x == POS_INVALID) continue;

		int x = state_disp[i].x * CELL_SIZE + OFFSET;
		int y = state_disp[i].y * CELL_SIZE + OFFSET;
		int centerX = x + CHIP_SIZE / 2;

		const char* fill_color = (pool.m_list[i]->m_Id.get() == CHIPID_GOTO) ? "#e9ecef" : "white";
		fprintf(fp, "  <rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" rx=\"4\" fill=\"%s\" stroke=\"#343a40\" stroke-width=\"2\" />\n",
				x, y, CHIP_SIZE, CHIP_SIZE, fill_color);
		
		fprintf(fp, "  <text x=\"%d\" y=\"%d\" font-family=\"Meiryo, sans-serif\" text-anchor=\"middle\">\n", centerX, y + 25);
		std::string txt = pool.m_list[i]->GetLayoutText();
		if (txt.empty()) {
			fprintf(fp, "    <tspan x=\"%d\" dy=\"1.2em\" font-size=\"10\" fill=\"#6c757d\">ID:%d</tspan>\n", centerX, i);
		} else {
			size_t s = 0, e = 0;
			int line_count = 0;
			while ((e = txt.find('\n', s)) != std::string::npos) {
				fprintf(fp, "    <tspan x=\"%d\" dy=\"%s\" font-size=\"11\" fill=\"#333\">%s</tspan>\n",
						centerX, (line_count == 0 ? "0" : "1.2em"), txt.substr(s, e - s).c_str());
				s = e + 1;
				line_count++;
			}
			fprintf(fp, "    <tspan x=\"%d\" dy=\"%s\" font-size=\"11\" fill=\"#333\">%s</tspan>\n",
					centerX, (line_count == 0 ? "0" : "1.2em"), txt.substr(s).c_str());
			fprintf(fp, "    <tspan x=\"%d\" dy=\"1.5em\" font-size=\"9\" fill=\"#999\">#%d</tspan>\n", centerX, i);
		}
		fprintf(fp, "  </text>\n");
	}

	// 5. 接続線の描画
	for (int i = 0; i < (int)pool.size(); ++i) {
		if (pool.m_list[i] == nullptr || state_disp[i].x == POS_INVALID) continue;

		double x1 = state_disp[i].x * CELL_SIZE + CELL_SIZE / 2.0;
		double y1 = state_disp[i].y * CELL_SIZE + CELL_SIZE / 2.0;

		auto process_conn = [&](UINT next_idx, const char* color) {
			if (next_idx == IDX_NONE) return;
			
			if (next_idx == IDX_EXIT) {
				// 枠外の仮想座標へ向けて描画
				double tx = x1, ty = y1;
				if (state_disp[i].x == 0) tx -= CELL_SIZE;
				else if (state_disp[i].x == pool.m_width - 1) tx += CELL_SIZE;
				else if (state_disp[i].y == 0) ty -= CELL_SIZE;
				else ty += CELL_SIZE;
				draw_physical_edge(x1, y1, tx, ty, color, true);
				return;
			}

			if (next_idx < pool.size() && state_disp[next_idx].x != POS_INVALID) {
				double x2 = state_disp[next_idx].x * CELL_SIZE + CELL_SIZE / 2.0;
				double y2 = state_disp[next_idx].y * CELL_SIZE + CELL_SIZE / 2.0;
				draw_physical_edge(x1, y1, x2, y2, color);
			}
		};

		process_conn(pool.m_list[i]->m_NextG, "#28a745");
		if (pool.m_list[i]->ValidR()) {
			process_conn(pool.m_list[i]->m_NextR, "#dc3545");
		}
	}

	fprintf(fp, "</svg>\n");
	fclose(fp);
}
//////////////////////////////////////////////////////////////////////////////

void chip_main(void);

int main(void){
	g_pCurTree		= new CChipTree;
	g_pCurChipPool	= new CChipPool(15, 15);
	g_pCurBlockInfo	= new CBlockInfo;
	
	try{
		chip_main();
	}catch(const OkeccError& e){
		puts(e.what());
		return 0;
	}
	
	g_pCurTree->AddToG(IDX_EXIT);
	g_pCurChipPool->m_start = g_pCurTree->m_start;
	//g_pCurChipPool->dump();
	
	// Goto 最適化
	g_pCurChipPool->CleanupGoto();
	
	printf("Number of chip(s): %d\n", (UINT)g_pCurChipPool->m_list.size());
	//g_pCurChipPool->dump();
	
	CarnageSA sa(*g_pCurChipPool, "chip.svg");
	sa.run();
	sa.SetArrow();
	sa.OutputSvg("chip.svg", sa.get_result());
	
	const std::string mcFile = "memcard1.mcd";
	const std::string gameId = "SLPS-01666";

	// 1. インスタンス生成（ファイル読み込みとID設定）
	MemoryCardManager manager(mcFile, gameId);

	// 2. データの読み出し
	std::vector<uint8_t> saveBuffer;
	size_t dataSize = manager.read(saveBuffer);

	if (dataSize == 0) {
		std::cerr << "Not found Game ID '" << gameId << std::endl;
		exit(1);
	}
	
	std::cout << "Game ID: " << gameId << " detected" << std::endl;
	std::cout << "Data size: " << dataSize << "Byte(s) (" << (dataSize / 1024) << " KB)" << std::endl;
	
	SaveDataZeus *pZeus = (SaveDataZeus *)saveBuffer.data();
	
	// ソフト書き込み
	constexpr int CARD = 0;
	
	auto& state = sa.get_result();
	for(UINT u = 0; u < 23 * 15; ++u) pZeus->Oke[CARD].Software[u] = 0;
	
	for(UINT u = 0; u < g_pCurChipPool->size(); ++u){
		CChipBinary bin;
		
		auto& p = state[u];
		g_pCurChipPool->m_list[u]->GetBin(bin);
		pZeus->Oke[CARD].Software[p.y * 23 + p.x] = bin.m_val;
	}
	
	pZeus->Oke[CARD].StartMainX = state[g_pCurChipPool->m_start].x + 2;
	pZeus->Oke[CARD].StartMainY = 2;
	
	// chksum
	uint8_t ChkSum = 0;
	for(int i = 0; i < dataSize - 1; ++i) ChkSum += saveBuffer[i];
	pZeus->ChkSum = ChkSum;

	// 3. データの書き込み
	if (manager.write(saveBuffer) && manager.saveToFile()) {
		std::cout << "Successfully wrote to " << mcFile << std::endl;
	} else {
		std::cerr << "Failed to write to " << mcFile << std::endl;
		return 1;
	}
	
	return 0;
}
