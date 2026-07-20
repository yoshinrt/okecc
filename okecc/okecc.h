#include <algorithm>
#include <concepts>
#include <format>
#include <source_location>
#include <stdexcept>
#include <vector>
#include <memory>
#include <functional>
#include <cmath>

#ifdef CHP
	inline constexpr bool g_bChp = 1;
	#define IF_CHP(p, e)	(p)
#else
	inline constexpr bool g_bChp = 0;
	#define IF_CHP(p, e)	(e)
#endif

enum {
	P_BULLET,
	P_BEAM,
	P_PULSE,
	P_NAPALM,
	P_GRENADE,
	P_BOMB,
	P_ROCKET,
	P_MISSILE,
	P_MINE,
	P_FMINE,
	P_HI_V,
	P_ALL,
};

enum {
	W_NONE,
	W_ASSAULT,
	W_BEAM,
	W_PULSE,
	W_NAPALM,
	W_FLAK,
	W_SHOTGUN,
	W_CANNON,
	W_RAILGUN,
	W_GRENADE,
	W_BOMB,
	W_ROCKET,
	W_MISSILE,
	W_MINE,
	W_FMINE,
#ifndef CHP
	W_SPECIAL,
	W_BROKEN,
#endif
};

enum {
	BC_UNLOCK,
	BC_BLOCKHEAD,
	BC_NEGRONI,
	BC_JAILOR,
	BC_EGGNOG,
	BC_AYAKAGE,
	BC_RUSTYNAIL,
	BC_NORLANDER,
	BC_TRINKER,
	BC_TSUKIKAGE,
	BC_CEMETERYKEEPER,
	BC_PARKDOG,
	BC_GRASSHOPPER,
	BC_ARACHNE,
	BC_NIDHOGG,
	BC_HADES,
	BC_CHICKENHUNTER,
	BC_TRIPOD,
	BC_DARKCOFFIN,
	BC_MARIAELENA,
	BC_ANUBIAS,
	BC_BADDREAM,
	BC_ROUKEN,
	BC_BASILISK,
	BC_HOYRYKONE,
	BC_ANGRIFF,
	BC_FRIED,
	BC_LOTUS,
	BC_PRIEST,
	BC_MOCKINGBIRD,
	BC_CHAYKA,
	BC_TARGETDRONE,
#ifdef CHP
	BC_VOLKS,
#else
	BC_AGRIOS,
	BC_CHIRON,
	BC_UNUSED34,
	BC_AVISPA,
	BC_GRAVESTONE,
	BC_GAZER,
#endif
	OKE_BIPED,
	OKE_QUADRUPED,
	OKE_HOVER,
	OKE_VEHICLE,
	OKE_FLIGHT,
	OKE_ALL,
	OKE_GROUND,
	OKE_TGT_GROUND	= OKE_ALL,
	OKE_TGT_ALL		= OKE_GROUND,
};

enum {
	BODY	= 0,
	
	W_ACTIVE	= 0,
	
	GS_OFF	= 6,
	GS_NEXT,
	
	AL_NOSOUND	= 0,
	AL_RIGHT	= 0,
	AL_LEFT,
	AL_TOP,
	AL_BOTTOM,
	AL_CENTER,
	AL_NONE,
};

typedef unsigned int UINT;

static constexpr UINT IDX_NONE = 0xFFFFFFFF;
static constexpr UINT IDX_EXIT = 0xFFFFFFFE;
constexpr int DEFAULT_INT = 0x7FFFFFFF;

extern void setCpuSize(int size);

//////////////////////////////////////////////////////////////////////////////

inline std::source_location g_LastLocation = std::source_location::current();
#define LastLocation()	(g_LastLocation = location)
#define LastLocationArg	const std::source_location location = std::source_location::current()

inline UINT g_uErrorCnt = 0;

static inline void Error(const std::string& message){
	puts(std::format("{}({}): Error: {}", g_LastLocation.file_name(), g_LastLocation.line(), message).c_str());
	++g_uErrorCnt;
}

static inline void Warning(const std::string& message){
	puts(std::format("{}({}): Warning: {}", g_LastLocation.file_name(), g_LastLocation.line(), message).c_str());
}

//////////////////////////////////////////////////////////////////////////////
// bit scaled int

class CChipBinary {
public:
	CChipBinary(uint64_t val = 0) : m_val(val), m_pos(0){}

	uint64_t GetVal(void) const { return m_val; }

	uint64_t	m_val;
	int			m_pos;
};

template<UINT _width = 8, int _min = DEFAULT_INT, int _max = DEFAULT_INT, UINT _step = 1, int _offset = 0>
class ScaledInt {
public:
	static constexpr UINT width			= _width;
	static constexpr uint32_t bitmask	= (1ULL << width) - 1;
	static constexpr int offset			= _offset;
	static constexpr UINT step			= _step;
	static constexpr int min			= _min == DEFAULT_INT ? offset : _min;
	static constexpr int max			= _max == DEFAULT_INT ? offset + (int)(step * bitmask) : _max;

	void set(int val){
		if(width < 32 && (val < min || max < val)){
			Error(std::format("Value {} is out of range [{} - {}]", val, min, max));
			return;
		}

		m_value = width < 32 ? ((((val - offset) << 1)/ step + 1) >> 1) & bitmask : val;
	};

	ScaledInt& operator=(int i){
		set(i);
		return *this;
	}

	int get(void){
		int i = m_value * step + offset;
		return width < 32 && (i > max) ? (i - max * 2) : i;
	}

	void GetBin(CChipBinary& bin){
		bin.m_val |= (uint64_t)(m_value & bitmask) << bin.m_pos;
		bin.m_pos += width;
	}

	void SetBin(CChipBinary& bin){
		m_value = (bin.m_val >> bin.m_pos) & bitmask;
		bin.m_pos += width;
	};

	UINT	m_value = 0;
};

//////////////////////////////////////////////////////////////////////////////
// Chip class
class CChip {
public:
	enum {
		CHIPID_NULL					= 0x00,
		CHIPID_NOP					= 0xC1,
		CHIPID_WAIT					= 0xC2,
		CHIPID_WAIT_AE				= 0xC3,
		CHIPID_SUB					= IF_CHP(0xDA, 0xC4),
		CHIPID_GET_STATUS			= IF_CHP(0xEA, 0xC5),
		CHIPID_CALC					= IF_CHP(0xEB, 0xC6),
		CHIPID_GETKEY				=              0xC9,
		CHIPID_CH_SEND				= IF_CHP(0xEF, 0xC8),
		CHIPID_CH_RECV				= IF_CHP(0xF0, 0xC7),
		CHIPID_IF_AMMO_NUM			= IF_CHP(0xD1, 0xCA),
		CHIPID_IF_OUTSIDE_AREA		= IF_CHP(0xD2, 0xCB),
		CHIPID_IF_BARRIER			= IF_CHP(0xD4, 0xCD),
		CHIPID_IF_OKE_NUM			= IF_CHP(0xD3, 0xCC),
		CHIPID_IF_DET_PROJECTILE	= IF_CHP(0xD5, 0xCE),
		CHIPID_IF_SELF_STATUS		= IF_CHP(0xD6, 0xCF),
		CHIPID_IF_MY_ACTION			= IF_CHP(0xD7, 0xD0),
		CHIPID_IF_TGT_ACTION		= IF_CHP(0xDE, 0xD5),
		CHIPID_IF_RAND				= IF_CHP(0xD8, 0xD1),
		CHIPID_IF_TGT_POS			= IF_CHP(0xDC, 0xD3),
		CHIPID_IF_POS_FROM_TGT		= IF_CHP(0xDD, 0xD4),
		CHIPID_IF_TIME				= IF_CHP(0xD9, 0xD2),
		CHIPID_TGT_BODYCODE			= IF_CHP(0xDF, 0xD6),
		CHIPID_NUM_LOCKED			= IF_CHP(0xE0, 0xD7),
		CHIPID_WEAPON_ID			= IF_CHP(0xE1, 0xD8),
		CHIPID_IS_LINE_CLEAR		= IF_CHP(0xE9, 0xD9),
		CHIPID_IF_CMP				= IF_CHP(0xEE, 0xDA),
		CHIPID_IF_KEY_STATUS		=              0xDB,
		CHIPID_IF_ALALOG_STATUS		=              0xDC,
		CHIPID_STOP					= IF_CHP(0xC4, 0xDD),
		CHIPID_MOVE					= IF_CHP(0xC5, 0xDE),
		CHIPID_TURN					= IF_CHP(0xC6, 0xDF),
		CHIPID_JUMP					= IF_CHP(0xC7, 0xE0),
		CHIPID_FAST_MOVE			= IF_CHP(0xC8, 0xE1),
		CHIPID_FAST_TURN			= IF_CHP(0xC9, 0xE2),
		CHIPID_FIGHT				= IF_CHP(0xCA, 0xE3),
		CHIPID_GUARD				= IF_CHP(0xCB, 0xE4),
		CHIPID_SPECIAL				= IF_CHP(0xCC, 0xE5),
		CHIPID_FIRE_NEAREST			= IF_CHP(0xCE, 0xE6),
		CHIPID_FIRE_FIXED_DIR		= IF_CHP(0xCF, 0xE7),
		CHIPID_AIM_GUNSIGHT_COUNTER	=              0xE8,
		CHIPID_GUNSIGHT_ON			=              0xE9,
		CHIPID_AIM_GUNSIGHT			=              0xEA,
		CHIPID_FIRE_TGT				= IF_CHP(0xE2, 0xEB),
		CHIPID_FIRE_COUNTER			=        0xED,
		CHIPID_JUMP_TURN			=        0xE3,
		CHIPID_MOVE_TURN			=        0xE4,
		CHIPID_FIRE_MOVE			=        0xE5,
		CHIPID_FIRE_JMP				=        0xE6,
		CHIPID_ALTITUDE				= IF_CHP(0xCD, 0xEC),
		CHIPID_OPTION				= IF_CHP(0xD0, 0xED),
		CHIPID_LOCKON				= IF_CHP(0xDB, 0xEE),
		CHIPID_LOCKON_COUNTER		= IF_CHP(0xEC, 0xF1),
		CHIPID_LOCKON_PARTS			= IF_CHP(0xE8, 0xF0),
		CHIPID_LOCK_RELEASE			=              0xF2,
		CHIPID_AUTO_TURN			= IF_CHP(0xE7, 0xEF),
		CHIPID_VIEW					=              0xF3,
		CHIPID_ALERT				=              0xF4,
		CHIPID_GOTO					= 0xFF,
	};

	static inline const char *m_OkeTypeStr[] = {
		"二脚",
		"四脚",
		"ホバー",
		"車両",
		"飛行",
		"全種",
		"地上型",
	};

	enum {
		OP_GE,
		OP_LE,
		OP_EQ,
	};

	static inline const char *m_operator_str[] = {
		"≧", "≦", "=="
	};

	enum {
		LEFT,
		RIGHT,
		FWD,
		BACK,
		FL,
		FR,
		BL,
		BR,
		UP,
	};

	static inline const char* m_move_str[] = {
		"左", "右", "前", "後", "左前", "右前", "左後", "右後", "上"
	};

	static inline const char *m_StatusTypeStr[] = {
		"敵数",
		"味方数",
		"経過時間",
		"rand",
		"自機X",
		"自機Y",
		"自機Z",
		"自機向き",
		"TGT ID",
		"TGT方向",
		"TGT仰角",
		"TGT X",
		"TGT Y",
		"TGT Z",
		"TGT向き",
		"TGT機体#",
		"TGT動作#",
		"TGT距離",
		"TGT距離(XY)",
		"残り時間",
		"自機ID",
		"自機動作#",
		"熱量",
		"HP",
		"燃料",
		"速度",
		"TGT速度",
		"武装1残弾",
		"武装2残弾",
		"武装3残弾",
		"武装4残弾",
		"武装5残弾",
		"Opt.1残弾",
		"Opt.2残弾",
		"Opt.3残弾",
		"Opt.4残弾",
		"探知距離",
		"π",
		"ボタン状態",
		"アナログX",
		"アナログY",
		"Gun方向",
		"Gun仰角",
		"Gun番号",
	};

	static inline const char *m_SelfStatusTypeStr[] = {
		"HP", "燃料", "熱量"
	};

	enum {
		ENEMY,
		FRIENDLY,
		ALL_OKE,
		UNKNOWN_OKE,
	};

	static inline const char *m_EnemyFriendlyStr[] = {
		"敵", "味方", "両軍", "不明",
	};

	enum {
		MY,
		TARGET
	};

	static inline const char *m_SelfTgtTypeStr[] = {
		"自機", "TGT"
	};

	enum {
		X, Y, Z
	};

	static inline const char *m_CoordinateStr[] = {
		"X", "Y", "Z"
	};

	static inline const char *m_VarNameStr[] = {
		"A", "B", "C", "D", "E", "F", "G", "H"
	};

	static inline const char *m_exmode_str[] = {
		"#", ""
	};

	static inline const char *m_fastmode_str[] = {
		"", "急"
	};

	enum {
		WAIT_STOP_ALL,
		WAIT_STOP_MOVE,
		WAIT_STOP_TURN,
		WAIT_STOP_FIRE,
	};
	
	static inline const char* m_WaitStopStr[] = {
		"全動作", "移動", "旋回", "射撃"
	};
	
	enum {
		EM_WAIT,
		EM_THROUGH,
	};

	enum {
		FM_NORMAL,
		FM_WIDE,
		FM_SNIPE,
	};

	CChip(){
		m_Id		= CHIPID_NULL;
		m_RawG		= 0;
		m_RawR		= 4;
	}

	virtual ~CChip(){}

	virtual std::string GetLayoutText(void){
		return "";
	}

	std::string GetDslText(void){
		std::string s = GetLayoutText();
		std::replace(s.begin(), s.end(), '\n', ' ');
		return s;
	}

	virtual void set_num(int num){}
	virtual void set_operator(int opr){}

	bool ValidG(void) const { return m_NextG < IDX_EXIT; }
	bool ValidR(void) const { return m_NextR < IDX_EXIT; }

	virtual void GetBin(CChipBinary& bin){
		m_Id.GetBin(bin);
		m_RawG.GetBin(bin);
		m_RawR.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		m_Id.SetBin(bin);
		m_RawG.SetBin(bin);
		m_RawR.SetBin(bin);
	}

	ScaledInt<>		m_Id;
	ScaledInt<4>	m_RawG;
	ScaledInt<4>	m_RawR;

	UINT			m_NextG = IDX_NONE;
	UINT			m_NextR = IDX_NONE;
	UINT			m_ChipId = 0;
	
	static constexpr double fix32p_max	= IF_CHP(99999.9, 99999.99);
	static constexpr int fix32p_scale	= IF_CHP(10, 100);
	
	static int double2i32(double val){
		if(-fix32p_max > val || val > fix32p_max){
			Error(std::format("Value {:.1f} is out of range [-fix32p_max - fix32p_max]", val));
		}
		
		return (int)std::round(val * fix32p_scale);
	}
	
	static std::string int2fixp32s(int val){
		if(val % fix32p_scale == 0) return std::format("{}", val / fix32p_scale);
		return std::format("{:.2f}", val / (double)fix32p_scale);
	}
};

class CChipTree;
class CChipCond : public CChip {
public:
	virtual ~CChipCond(){}

	CChipTree operator>=(int num);
	CChipTree operator<=(int num);
	CChipTree operator> (int num);
	CChipTree operator< (int num);
	CChipTree operator==(int num);
	CChipTree operator!=(int num);
	CChipTree operator! ();

	operator CChipTree();

	CChipTree GetChipTree();

	bool m_cond_eq = false;
};

//////////////////////////////////////////////////////////////////////////////
// NOP

class CChipNop : public CChip {
	public:
	CChipNop(){
		m_Id = CHIPID_NOP;
	}
	virtual ~CChipNop(){}
	virtual std::string GetLayoutText(void){
		return "NOP";
	}
};

//////////////////////////////////////////////////////////////////////////////
// ゲームには存在しない仮チップ

class CChipGoto : public CChip {
public:
	CChipGoto(){
		m_Id = CHIPID_GOTO;
	}

	virtual ~CChipGoto(){}
};

//////////////////////////////////////////////////////////////////////////////
// Chip pool

class CChipPool {
public:
	std::vector<std::unique_ptr<CChip>> m_list;
	UINT m_start = IDX_NONE;	// 開始チップの index
	UINT m_width;
	UINT m_height;

	CChipPool(UINT width, UINT height) : m_width(width), m_height(height) {}

	UINT add(std::unique_ptr<CChip> chip){
		auto p = chip.get();

		m_list.push_back(std::move(chip));
		p->m_ChipId = (UINT)m_list.size() - 1;
		return p->m_ChipId;
	}

	size_t size(void) const { return m_list.size(); }

	CChip* operator[](size_t index) const {
		return m_list[index].get();
	}

	// Goto を辿って最終地点を取得
	UINT GetFinalDst(UINT idx){
		UINT dst = idx;

		// 最終地点を取得
		while(dst < m_list.size() && m_list[dst]->m_Id.get() == CChip::CHIPID_GOTO) dst = m_list[dst]->m_NextG;

		// 最終地点までの Goto はすべて最終地点を指す
		while(idx != dst){
			UINT next = m_list[idx]->m_NextG;
			m_list[idx]->m_NextG = dst;
			idx = next;
		}

		return dst;
	}

	// Goto 最適化
	void CleanupGoto(void){
		// Goto 飛び先解決
		std::vector<UINT> IdxOld2New;
		std::vector<UINT> IdxNew2Old;

		for(UINT u = 0; u < m_list.size(); ++u){
			if(u==0x2c){
				int a = 0;
			}
			if(m_list[u]->m_Id.get() != CChip::CHIPID_GOTO){
				m_list[u]->m_NextR = GetFinalDst(m_list[u]->m_NextR);
				m_list[u]->m_NextG = GetFinalDst(m_list[u]->m_NextG);
				IdxNew2Old.push_back(u);
			}
			IdxOld2New.push_back((UINT)IdxNew2Old.size() - 1);
		}
		m_start = GetFinalDst(m_start);

		//Goto 削除
		for(UINT u = 0; u < m_list.size(); ++u){
			if(m_list[u]->m_Id.get() == CChip::CHIPID_GOTO) m_list[u].reset();
		}

		// 空きを詰める
		for(UINT u = 0; u < IdxNew2Old.size(); ++u){
			m_list[u] = std::move(m_list[IdxNew2Old[u]]);
			if(m_list[u]->ValidG()) m_list[u]->m_NextG = IdxOld2New[m_list[u]->m_NextG];
			if(m_list[u]->ValidR()) m_list[u]->m_NextR = IdxOld2New[m_list[u]->m_NextR];
		}
		m_start = IdxOld2New[m_start];
		m_list.resize(IdxNew2Old.size());
	}

	// 参照されないチップを goto に置き換えることで，goto 最適化で削除する
	void DeleteUnreferencedChips(void){
		std::vector<bool> referenced(m_list.size(), false);

		// 参照されているチップをマーク
		std::function<void(UINT)> mark_referenced = [&](UINT idx){
			if(idx >= m_list.size() || referenced[idx]) return;
			referenced[idx] = true;
			if(m_list[idx]->ValidG()) mark_referenced(m_list[idx]->m_NextG);
			if(m_list[idx]->ValidR()) mark_referenced(m_list[idx]->m_NextR);
		};

		mark_referenced(m_start);

		// 参照されていないチップを Goto に置き換える
		for(UINT u = 0; u < m_list.size(); ++u){
			if(!referenced[u]){
				m_list[u]->m_Id = CChip::CHIPID_GOTO;
			}
		}
	}

	// 自分に分岐しているチップは nop を挟む
	void FixSelfReference(void){
		for(UINT u = 0; u < m_list.size(); ++u){
			if(m_list[u]->m_NextG == u){
				UINT idx = add(std::make_unique<CChipNop>());
				m_list[u]->m_NextG = idx;
				m_list[idx]->m_NextG = u;
			}
			if(m_list[u]->m_NextR == u){
				UINT idx = add(std::make_unique<CChipNop>());
				m_list[u]->m_NextR = idx;
				m_list[idx]->m_NextG = u;
			}
		}
	}

	// チップに突入する数が多いものを分割する
	void SplitFanin(void){
		auto numChip = m_list.size();
		std::vector<std::vector<UINT>> fanin(numChip);

		// その chip に突入する chip の index を集める
		for(UINT u = 0; u < numChip; ++u){
			if(m_list[u]->ValidG()) fanin[m_list[u]->m_NextG].push_back(u);
			if(m_list[u]->ValidR()) fanin[m_list[u]->m_NextR].push_back(u);
		}

		// NextG/R を付け替える
		auto fixNext = [&](UINT chip, UINT oldIdx, UINT newIdx){
			if(m_list[chip]->m_NextG == oldIdx) m_list[chip]->m_NextG = newIdx;
			if(m_list[chip]->m_NextR == oldIdx) m_list[chip]->m_NextR = newIdx;
		};

		// fanin が多い chip を分割する
		for(UINT chip = 0; chip < numChip; ++chip){
			if(fanin[chip].size() >= 6){
				// 追加する nop 数
				int numNop = ((int)fanin[chip].size() - 2) / 2;

				UINT NopIdx = chip, PrevNopIdx;
				for(int i = 1; i < numNop; ++i){
					PrevNopIdx = NopIdx;
					NopIdx = add(std::make_unique<CChipNop>());
					m_list[NopIdx]->m_NextG = PrevNopIdx;

					// 途中の nop は 2chip だけ受け入れ
					fixNext(fanin[chip][i], chip, NopIdx);
					fixNext(fanin[chip][fanin[chip].size() - i - 1], chip, NopIdx);

					// 最後の nop は残りすべてを受け入れる
					if(i == numNop - 1){
						for(int j = numNop; j < fanin[chip].size() - numNop; ++j){
							fixNext(fanin[chip][j], chip, NopIdx);
						}
					}
				}
			}
		}
	}

	int Finalize(void){
		if(m_list.size() > 0){
			DeleteUnreferencedChips();	// 参照されないチップを goto に置き換える
			CleanupGoto();				// Goto 最適化
			FixSelfReference();			// 自分に分岐しているチップは nop を挟む
			SplitFanin();				// fanin が多い chip を分割する

			// field 最大チップ数を超えていたらエラー
			if(m_list.size() > m_width * m_height){
				printf("Number of chip(s) exceeds the maximum limit: %d > %d\n",
					(int)m_list.size(),
					m_width * m_height
				);

				return -1;
			}
		}

		return (int)m_list.size();
	}

	void dump(void){
		printf("Start=%d\n ID   G   R  Description\n", m_start);
		for(UINT u = 0; u < m_list.size(); ++u){
			if(m_list[u]){
				std::string s = m_list[u]->GetLayoutText();
				std::replace(s.begin(), s.end(), '\n', ' ');

				printf("%3d %3d %3d  %s\n",
					u,
					m_list[u]->m_NextG,
					m_list[u]->m_NextR,
					s.c_str()
				);
			}
		}
	}
};

//////////////////////////////////////////////////////////////////////////////
// Chip tree

class CChipTree {
public:

	UINT m_start	= IDX_NONE;	// 開始 chip
	UINT m_LastG	= IDX_NONE;	// 最後の緑矢印を出している chip
	UINT m_LastR	= IDX_NONE; // 最後の赤矢印を出している chip

	CChipPool&	m_pool;

	CChipTree(CChipPool& pool) : m_pool(pool){}

	CChipTree(UINT ChipId, CChipPool& pool) : m_pool(pool){
		m_start = ChipId;
		m_LastG = m_start;

		// R 側に Goto を足して，常に m_NextG を触ればいいようにする
		m_LastR = m_pool.add(std::make_unique<CChipGoto>());
		m_pool[m_start]->m_NextR = m_LastR;
	}

	// g, r を update
	void AddToG(UINT idx){
		if(m_start == IDX_NONE) m_start = idx;
		if(ValidG()) m_pool[m_LastG]->m_NextG = idx;
		m_LastG = idx;
	}

	void AddToR(UINT idx){
		if(m_start == IDX_NONE) m_start = idx;
		if(ValidR()) m_pool[m_LastR]->m_NextG = idx;
		m_LastR = idx;
	}

	bool ValidStart(void) const {return m_start < m_pool.m_list.size();}
	bool ValidG    (void) const {return m_LastG < m_pool.m_list.size();}
	bool ValidR    (void) const {return m_LastR < m_pool.m_list.size();}

	// Tree にチップ追加
	CChip* AddToTreeBase(std::unique_ptr<CChip> chip) {
		UINT idx = m_pool.add(std::move(chip));		// チップ追加

		if(ValidStart()){
			m_pool[m_LastG]->m_NextG = idx;	// リスト後端に追加したチップをつなげる
		}else{
			m_start = idx;
		}
		m_LastG = idx;

		return m_pool[idx];
	}

	template<typename T>
	T* AddToTree(std::unique_ptr<T> chip) {
		return static_cast<T*>(AddToTreeBase(std::move(chip)));
	}

	// Pool にチップ追加，ツリーには連結しない CChipCond 用
	CChip* AddToPoolBase(std::unique_ptr<CChip> chip) {
		UINT idx = m_pool.add(std::move(chip));		// チップ追加
		return m_pool[idx];
	}

	template<typename T>
	T* AddToPool(std::unique_ptr<T> chip) {
		return static_cast<T*>(AddToPoolBase(std::move(chip)));
	}

	CChipTree operator&&(const CChipTree& b) const {
		CChipTree cc_a = *this;
		CChipTree cc_b = b;

		// a.r -> cc_b.start を指す
		cc_a.AddToR(cc_b.m_start);

		// False 側: GOTO を生成し a.g, cc_b.g, tree.g はそれを指す
		UINT idx = m_pool.add(std::make_unique<CChipGoto>());
		cc_a.AddToG(idx);
		cc_b.AddToG(idx);

		// True 側: tree.r は cc_b.r になる
		cc_a.m_LastR = cc_b.m_LastR;

		return cc_a;
	}

	CChipTree operator||(const CChipTree& b) const {
		CChipTree cc_a = *this;
		CChipTree cc_b = b;

		// a.g -> cc_b.start を指す
		cc_a.AddToG(cc_b.m_start);

		// True 側: GOTO を生成し a.r, cc_r.g, tree.r はそれを指す
		UINT idx = m_pool.add(std::make_unique<CChipGoto>());
		cc_a.AddToR(idx);
		cc_b.AddToR(idx);

		// False 側: tree.g は cc_b.g になる
		cc_a.m_LastG = cc_b.m_LastG;

		return cc_a;
	}

	CChipTree& operator>=(int num) {
		m_pool[m_start]->set_num(num);
		m_pool[m_start]->set_operator(CChip::OP_GE);
		return *this;
	}

	CChipTree& operator<=(int num) {
		m_pool[m_start]->set_num(num);
		m_pool[m_start]->set_operator(CChip::OP_LE);
		return *this;
	}

	CChipTree operator>(int num) {
		return !(*this <= num);
	}

	CChipTree operator<(int num) {
		return !(*this >= num);
	}

	CChipTree& operator==(int num) {
		m_pool[m_start]->set_num(num);
		return *this;
	}

	CChipTree operator!=(int num) {
		return !(*this == num);
	}

	CChipTree operator!(void) const {
		auto cc = *this;
		cc.m_LastG = m_LastR;
		cc.m_LastR = m_LastG;

		return cc;
	}
};

//////////////////////////////////////////////////////////////////////////////
// field info

class CField {
public:
	CChipPool	m_pool;
	CChipTree	m_tree;
	const char	*m_name;

	UINT m_uISubRetIdx = IDX_EXIT;

	CField(const char *name, UINT width, UINT height) : m_name(name), m_pool(width, height), m_tree(m_pool){}

	int FinalizeCompile(){
		// start / end 最終設定
		m_tree.AddToG(IDX_EXIT);
		m_pool.m_start = m_tree.m_start;
		//m_pool.dump();

		auto ret = m_pool.Finalize();
		if(ret >= 0) printf("%s: Number of chip(s): %d\n", m_name, (int)m_pool.m_list.size());
		return ret;
	}

	// ブロック制御

	enum {
		BM_NONE,
		BM_IF_TOP,	// top of if block
		BM_IF,		// if / elseif 中
		BM_ELSE,	// else 中
		BM_LOOP,	// loop 中
	};

	class BlockInfo {
	public:
		std::source_location m_location;
		UINT m_TargetIdx;

		BlockInfo(std::source_location location, UINT idx = IDX_NONE) : m_location(location), m_TargetIdx(idx){}
		virtual ~BlockInfo(){}

		virtual UINT GetMode(){return BM_NONE;}
	};

	class BiIfTop : public BlockInfo {
	public:
		BiIfTop(std::source_location location, UINT idx = IDX_NONE) : BlockInfo(location, idx){}
		virtual ~BiIfTop(){}
		virtual UINT GetMode(){return BM_IF_TOP;}
	};

	class BiIf : public BlockInfo {
	public:
		BiIf(std::source_location location, UINT idx) : BlockInfo(location, idx){}
		virtual ~BiIf(){}
		virtual UINT GetMode(){return BM_IF;}
	};

	class BiElse : public BlockInfo {
	public:
		BiElse(std::source_location location, UINT idx) : BlockInfo(location, idx){}
		virtual ~BiElse(){}
		virtual UINT GetMode(){return BM_ELSE;}
	};

	class BiLoop : public BlockInfo {
	public:
		BiLoop(std::source_location location, UINT idx, UINT brk) : BlockInfo(location, idx), m_SavePrevBreak(brk){}

		virtual ~BiLoop(){}
		virtual UINT GetMode(){return BM_LOOP;}

		UINT m_SavePrevBreak;
	};

	static constexpr UINT BLOCK_TOP = 0xFFFFFFFF;

	std::vector<std::unique_ptr<BlockInfo>>	m_BlockStack;
	UINT m_Break = IDX_NONE;

	UINT GetMode(void){
		return m_BlockStack.size() < 1 ? BM_NONE :
			m_BlockStack.back()->GetMode();
	}

	void CheckBlockStack(){
		while(m_BlockStack.size() > 0){
			g_LastLocation = m_BlockStack.back()->m_location;
			Error("Unterminated block statement");
			m_BlockStack.pop_back();
		}
	}

	std::string GetBlockErrorMsg(const std::string& message){
		if(m_BlockStack.size() == 0) return message;

		return std::format("{}\n  Corresponging statement: {}({})",
			message,
			m_BlockStack.back()->m_location.file_name(),
			m_BlockStack.back()->m_location.line()
		);
	}

	void BlockError(const std::string& message){
		Error(GetBlockErrorMsg(message));
	}
};

inline std::vector<std::unique_ptr<CField>>	g_pField;
inline CField *g_pCurField;

//////////////////////////////////////////////////////////////////////////////
// CChipCond --> CChipTree operator

#ifndef NO_OKECC_SYNTAX
CChipTree CChipCond::operator>=(int num){
	if (m_cond_eq) Error("Use '==' or '!=' for equality comparison");
	return GetChipTree() >= num;
}

CChipTree CChipCond::operator<=(int num){
	if (m_cond_eq) Error("Use '==' or '!=' for equality comparison");
	return GetChipTree() <= num;
}

CChipTree CChipCond::operator>(int num){
	if (m_cond_eq) Error("Use '==' or '!=' for equality comparison");
	return !(GetChipTree() <= num);
}

CChipTree CChipCond::operator<(int num){
	if (m_cond_eq) Error("Use '==' or '!=' for equality comparison");
	return !(GetChipTree() >= num);
}

CChipTree CChipCond::operator==(int num){
	if (!m_cond_eq) Error("Use '>=' or '<=' for inequality comparison");
	return GetChipTree() == num;
}

CChipTree CChipCond::operator!=(int num) {
	if (!m_cond_eq) Error("Use '>=' or '<=' for inequality comparison");
	return GetChipTree() != num;
}

CChipTree CChipCond::operator!() {
	if (m_cond_eq) Error("Use '==' or '!=' for equality comparison");
	return GetChipTree() < 1;
}

CChipTree CChipCond::GetChipTree(){
	return CChipTree(this->m_ChipId, g_pCurField->m_pool);
}

inline CChipCond::operator CChipTree(){
    return GetChipTree();
}
#endif

//////////////////////////////////////////////////////////////////////////////

// CChipTree に変換可能な型を CChipTree として扱うテンプレート
template <typename T>
concept ChipLike = std::convertible_to<T, CChipTree>;

template <ChipLike T, ChipLike U>
requires (!std::same_as<std::remove_cvref_t<T>, CChipTree> || !std::same_as<std::remove_cvref_t<U>, CChipTree>)
CChipTree operator&&(T&& lhs, U&& rhs) {
	return static_cast<CChipTree>(std::forward<T>(lhs)) &&
		   static_cast<CChipTree>(std::forward<U>(rhs));
}

template <ChipLike T, ChipLike U>
requires (!std::same_as<std::remove_cvref_t<T>, CChipTree> || !std::same_as<std::remove_cvref_t<U>, CChipTree>)
CChipTree operator||(T&& lhs, U&& rhs) {
	return static_cast<CChipTree>(std::forward<T>(lhs)) ||
		   static_cast<CChipTree>(std::forward<U>(rhs));
}

//////////////////////////////////////////////////////////////////////////////

class CChipVal {
public:
	enum {
		FRIENDLY,
		ENEMY,
		TIME,
		RAND,
		MY_POS_X,
		MY_POS_Y,
		MY_POS_Z,
		MY_DIRECTION,
		TGT_NO,
		TGT_AZIMUTH,
		TGT_ELEVATION,
		TGT_POS_X,
		TGT_POS_Y,
		TGT_POS_Z,
		TGT_DIRECTION,
		TGT_BODYCODE,
		TGT_ACTCODE,
		TGT_DISTANCE,
		TGT_DISTANCE_XY,
		TIME_REMAINED,
		MY_ID,
		MY_ACTCODE,
		MY_HEAT,
		MY_HP,
		MY_ENERGY,
		MY_SPEED,
		TGT_SPEED,
		NUM_AMMO1,
		NUM_AMMO2,
		NUM_AMMO3,
		NUM_AMMO4,
		NUM_AMMO5,
		NUM_OPTION1,
		NUM_OPTION2,
		NUM_OPTION3,
		NUM_OPTION4,
		RADAR_RNAGE,
		PI,
		BUTTON,
		ANALOG_X,
		ANALOG_Y,
		GUNSIGHT_DIRECTION,
		GUNSIGHT_ELEVATION,
		GUNSIGHT_NO,

		NUM_AMMO0,
		MATH,
		CH_RECV,
	};

	CChipVal(UINT type, int param = 0) : m_type(type), m_param(param){}
	CChipVal(UINT type, std::unique_ptr<CChip> chip) : m_type(type), m_chip(std::move(chip)) {}

	CChipTree operator!(void) const {
		return !(*this >= 1);
	}

	operator CChipTree() const {
		return *this >= 1;
	}

	auto& GetCChipCond(void) const;
	CChipTree operator<=(int num) const;
	CChipTree operator< (int num) const;
	CChipTree operator>=(int num) const;
	CChipTree operator> (int num) const;
	CChipTree operator==(int num) const;
	CChipTree operator!=(int num) const;

	UINT m_type;
	int m_param;

	std::unique_ptr<CChip> m_chip = nullptr;
};

//////////////////////////////////////////////////////////////////////////////
// 変数

class CChipVar {
public:
	CChipVar(UINT var) : m_var(var){}
	UINT m_var;

	CChipVar& operator=(const CChipVal& chip);

	operator CChipTree() const {
		return *this != 0;
	}

	CChipTree operator!(void) const {
		return *this == 0;
	}

	CChipVar& operator+=(const CChipVar& op2);
	CChipVar& operator-=(const CChipVar& op2);
	CChipVar& operator*=(const CChipVar& op2);
	CChipVar& operator/=(const CChipVar& op2);
	CChipVar& operator%=(const CChipVar& op2);
	CChipVar& operator= (const CChipVar& op2);
	CChipVar& operator&=(const CChipVar& op2);
	CChipVar& operator|=(const CChipVar& op2);
	CChipVar& operator^=(const CChipVar& op2);

	CChipVar& operator+=(const double op2);
	CChipVar& operator-=(const double op2);
	CChipVar& operator*=(const double op2);
	CChipVar& operator/=(const double op2);
	CChipVar& operator%=(const double op2);
	CChipVar& operator= (const double op2);
	CChipVar& operator&=(const double op2);
	CChipVar& operator|=(const double op2);
	CChipVar& operator^=(const double op2);

	CChipVar& operator++();
	CChipVar& operator--();

	CChipTree operator>=(const CChipVar& op2) const;
	CChipTree operator<=(const CChipVar& op2) const;
	CChipTree operator==(const CChipVar& op2) const;
	CChipTree operator!=(const CChipVar& op2) const;
	CChipTree operator> (const CChipVar& op2) const;
	CChipTree operator< (const CChipVar& op2) const;

	CChipTree operator>=(const double imm) const;
	CChipTree operator<=(const double imm) const;
	CChipTree operator==(const double imm) const;
	CChipTree operator!=(const double imm) const;
	CChipTree operator> (const double imm) const;
	CChipTree operator< (const double imm) const;
};

inline CChipVar A(0);
inline CChipVar B(1);
inline CChipVar C(2);
inline CChipVar D(3);
inline CChipVar E(4);
inline CChipVar F(5);
inline CChipVar G(6);
inline CChipVar H(7);

//////////////////////////////////////////////////////////////////////////////
// simple chip

static void _nop(
	LastLocationArg
){
	LastLocation();
	g_pCurField->m_tree.AddToTree(std::make_unique<CChipNop>());
}

class CChipWaitAE : public CChip {
public:
	CChipWaitAE(UINT param){
		m_Id = CHIPID_WAIT_AE;
		m_param = param;
	}
	virtual ~CChipWaitAE(){}
	
	virtual std::string GetLayoutText(void){
		return std::format("{}\n完了待ち", m_WaitStopStr[m_param.get()]);
	}
	
	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_param.GetBin(bin);
	}
	
	ScaledInt<8, 0, 3> m_param;
};

static void wait_chip(
	UINT param,
	LastLocationArg
) {
	LastLocation();
	g_pCurField->m_tree.AddToTree(std::make_unique<CChipWaitAE>(param));
}

static void _wait    (LastLocationArg){wait_chip(CChipWaitAE::WAIT_STOP_ALL,  location);}

#ifndef CHP
static void _waitMove(LastLocationArg){wait_chip(CChipWaitAE::WAIT_STOP_MOVE, location);}
static void _waitTurn(LastLocationArg){wait_chip(CChipWaitAE::WAIT_STOP_TURN, location);}
static void _waitFire(LastLocationArg){wait_chip(CChipWaitAE::WAIT_STOP_FIRE, location);}
#endif

class CChipWait : public CChip {
public:
	CChipWait(UINT param = 0){
		m_Id	= CHIPID_WAIT;
		m_param	= param;
	}

	virtual ~CChipWait(){}

	virtual std::string GetLayoutText(void){
		return std::format("Stop\n{}/30s", m_param.get());
	}

	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_param.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		m_param.SetBin(bin);
	}

	ScaledInt<8, 1, 120> m_param;
};

static void sleep(
	UINT param,
	LastLocationArg
){
	LastLocation();
	g_pCurField->m_tree.AddToTree(std::make_unique<CChipWait>(param));
}

//////////////////////////////////////////////////////////////////////////////
// Stop

class CChipStop : public CChip {
public:
	CChipStop(UINT param){
		m_Id = CHIPID_STOP;
		m_param = param;
	}
	virtual ~CChipStop(){}
	
	virtual std::string GetLayoutText(void){
		return std::format("{}\n停止", m_WaitStopStr[m_param.get()]);
	}
	
	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_param.GetBin(bin);
	}
	
	ScaledInt<> m_param;
};

static void stop_chip(
	UINT param,
	LastLocationArg
) {
	LastLocation();
	g_pCurField->m_tree.AddToTree(std::make_unique<CChipStop>(param));
}

static void _stop    (LastLocationArg){stop_chip(CChipWaitAE::WAIT_STOP_ALL,  location);}

#ifndef CHP
static void _stopMove(LastLocationArg){stop_chip(CChipWaitAE::WAIT_STOP_MOVE, location);}
static void _stopTurn(LastLocationArg){stop_chip(CChipWaitAE::WAIT_STOP_TURN, location);}
static void _stopFire(LastLocationArg){stop_chip(CChipWaitAE::WAIT_STOP_FIRE, location);}
#endif

//////////////////////////////////////////////////////////////////////////////
// Button

#ifndef CHP
class CChipGetButton : public CChip {
public:
	CChipGetButton(UINT param = 0){
		m_Id		= CHIPID_GETKEY;
		m_exmode	= EM_THROUGH;
	}

	virtual ~CChipGetButton(){}

	virtual std::string GetLayoutText(void){
		return std::format("{}Button", m_exmode_str[m_exmode.get()]);
	}

	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_exmode.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		m_exmode.SetBin(bin);
	}
	
	// option
	auto& _wait()		{m_exmode	= EM_WAIT;	return *this;}
	
	ScaledInt<>	m_exmode;
};

static auto& _getButton(
	LastLocationArg
){
	LastLocation();
	return *g_pCurField->m_tree.AddToTree(std::make_unique<CChipGetButton>());
}

enum {
	BT_UP			= 1 << 0,
	BT_DOWN			= 1 << 1,
	BT_RIGHT		= 1 << 2,
	BT_LEFT			= 1 << 3,
	BT_CIRCLE		= 1 << 4,
	BT_CROSS		= 1 << 5,
	BT_TRIANGLE		= 1 << 6,
	BT_SQUARE		= 1 << 7,
	BT_L			= 1 << 8,
	BT_R			= 1 << 9,
	BT_PUSH_ALL		= 0x0 << 16,
	BT_PUSH_ANY		= 0x1 << 16,
	BT_RELEASE_ALL	= 0x2 << 16,
	BT_RELEASE_ANY	= 0x3 << 16,
	BT_CHANGE		= 0x4 << 16,
};

class CChipIsButton : public CChipCond {
public:
	static inline const char* m_KeyStr[] = {
		"全て押", "何れか押", "全て離", "何れか離", "何れか変"
	};

	CChipIsButton(
		int key
	){
		m_Id	= CHIPID_IF_KEY_STATUS;
		m_key	= key;
	}
	
	virtual ~CChipIsButton(){}

	virtual std::string GetLayoutText(void){
		return
			std::format(
				"ボタン\n{:03X}\n{}",
				m_key.get() & 0x3FF,
				m_KeyStr[m_key.get() >> 16]
			);
	}

	virtual void GetBin(CChipBinary& bin){
		CChipCond::GetBin(bin);
		m_key.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChipCond::SetBin(bin);
		m_key.SetBin(bin);
	}

	ScaledInt<24>		m_key;
};

static auto& isButton(
	int key
){
	return *g_pCurField->m_tree.AddToPool(std::make_unique<CChipIsButton>(key));
}

class CChipIsAnalog : public CChipCond {
public:
	enum{
		X, Y
	};
	
	CChipIsAnalog(int pad){
		m_Id	= CHIPID_IF_ALALOG_STATUS;
		m_pad	= pad;
	}
	
	virtual ~CChipIsAnalog(){}

	virtual void set_num(int num){m_num = num;}
	virtual void set_operator(int opr){m_operator = opr;}
	
	virtual std::string GetLayoutText(void){
		return
			std::format(
				"アナログ{}\n{}{}",
				(m_pad.get() == X ? "X" : "Y"),
				m_operator_str[m_operator.get()], (int8_t)m_num.get()
			);
	}

	virtual void GetBin(CChipBinary& bin){
		CChipCond::GetBin(bin);
		m_pad.GetBin(bin);
		m_num.GetBin(bin);
		m_operator.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChipCond::SetBin(bin);
		m_pad.SetBin(bin);
		m_num.SetBin(bin);
		m_operator.SetBin(bin);
	}

	ScaledInt<>				m_pad;
	ScaledInt<8, -128, 127>	m_num;
	ScaledInt<>				m_operator;
};

#endif

//////////////////////////////////////////////////////////////////////////////
// move

class CChipMove : public CChip {
public:
	enum {
		NORMAL,
		FAST,
		TURN,
	};

	CChipMove(int param){
		m_Id		= CHIPID_MOVE;
		m_param		= param;
		m_exmode	= EM_THROUGH;
	}

	virtual ~CChipMove(){}

	virtual std::string GetLayoutText(void){
		std::string s = std::format("{}{}{}移動", m_exmode_str[m_exmode.get()], m_fastmode_str[m_mode], m_move_str[m_param.get()]);
		
		if(m_mode == NORMAL && !g_bChp){
			if(m_dist.get() == 0){
				s += "\n∞";
			}else{
				s += std::format("\n{}m", m_dist.get());
			}
		}
		if(m_mode == TURN){
			s += std::format("\n+{}旋回", m_move_str[m_turn.get()]);
		}

		return s;
	}

	virtual void GetBin(CChipBinary& bin){
		if(     m_mode == FAST) m_Id = CHIPID_FAST_MOVE;
		else if(m_mode == TURN) m_Id = CHIPID_MOVE_TURN;

		CChip::GetBin(bin);
		m_param.GetBin(bin);
		if(m_mode == TURN) m_turn.GetBin(bin);
		if(m_mode == FAST) m_exmode.GetBin(bin);
		
		if(m_mode == NORMAL && !g_bChp){
			m_exmode.GetBin(bin);
			m_dist.GetBin(bin);
		}
	}

	// option
	auto& _fast()		{m_mode		= FAST;		return *this;}
	auto& _wait()		{m_exmode	= EM_WAIT;	return *this;}
	#ifdef CHP
		auto& _turnLeft()	{m_turn	= LEFT;  m_mode = TURN; return *this;}
		auto& _turnRight()	{m_turn	= RIGHT; m_mode = TURN; return *this;}
	#else
		auto& dist(int dist){m_dist = dist; m_mode = NORMAL; return *this;}
	#endif
	
	ScaledInt<>				m_param;
	ScaledInt<>				m_turn;
	ScaledInt<>				m_exmode;
	ScaledInt<16, 0, 500>	m_dist;

	int	m_mode = NORMAL;
};

static auto& put_move_chip(int param, LastLocationArg) {
	return *g_pCurField->m_tree.AddToTree(std::make_unique<CChipMove>(param));
}

static auto& _moveLeft			(LastLocationArg){LastLocation(); return put_move_chip(CChip::LEFT);}
static auto& _moveRight			(LastLocationArg){LastLocation(); return put_move_chip(CChip::RIGHT);}
static auto& _moveForward		(LastLocationArg){LastLocation(); return put_move_chip(CChip::FWD);}
static auto& _moveBackward		(LastLocationArg){LastLocation(); return put_move_chip(CChip::BACK);}
static auto& _moveForwardLeft	(LastLocationArg){LastLocation(); return put_move_chip(CChip::FL);}
static auto& _moveForwardRight	(LastLocationArg){LastLocation(); return put_move_chip(CChip::FR);}
static auto& _moveBackwardLeft	(LastLocationArg){LastLocation(); return put_move_chip(CChip::BL);}
static auto& _moveBackwardRight	(LastLocationArg){LastLocation(); return put_move_chip(CChip::BR);}

//////////////////////////////////////////////////////////////////////////////
// turn

class CChipTurn : public CChip {
public:
	CChipTurn(int param){
		m_Id		= CHIPID_TURN;
		m_param		= param;
		m_exmode	= EM_THROUGH;
	}

	virtual ~CChipTurn(){}

	virtual std::string GetLayoutText(void){
		std::string s = std::format("{}{}{}旋回", m_exmode_str[m_exmode.get()], m_fastmode_str[m_fast], m_move_str[m_param.get()]);
		
		if(!m_fast && !g_bChp){
			if(m_angle.get() == 0){
				s += "\n∞";
			}else{
				s += std::format("\n{}°", m_angle.get());
			}
		}
		
		return s;
	}

	virtual void GetBin(CChipBinary& bin){
		if(m_fast) m_Id = CHIPID_FAST_TURN;
		CChip::GetBin(bin);
		m_param.GetBin(bin);
		if(m_fast) m_exmode.GetBin(bin);
		else if(!g_bChp){
			m_exmode.GetBin(bin);
			m_angle.GetBin(bin);
		}
	}

	// option
	auto& _fast()		{m_fast		= 1;		return *this;}
	auto& _wait()		{m_exmode	= EM_WAIT;	return *this;}
	#ifndef CHP
		auto& angle(int angle){m_angle = angle; m_fast = 0; return *this;}
	#endif

	ScaledInt<>				m_param;
	ScaledInt<>				m_exmode;
	ScaledInt<16, 0, 360>	m_angle;

	int	m_fast = 0;
};

static auto& put_turn_chip(int param, LastLocationArg){
	return *g_pCurField->m_tree.AddToTree(std::make_unique<CChipTurn>(param));
}

static auto& _turnLeft	(LastLocationArg){LastLocation(); return put_turn_chip(CChip::LEFT);}
static auto& _turnRight	(LastLocationArg){LastLocation(); return put_turn_chip(CChip::RIGHT);}

//////////////////////////////////////////////////////////////////////////////
// Jump

class CChipJump : public CChip {
public:
	CChipJump(int param){
		m_Id = CHIPID_JUMP;
		m_param = param;
		m_exmode = EM_THROUGH;
	}

	virtual ~CChipJump() {}

	virtual std::string GetLayoutText(void) {
		std::string s = std::format("{}{}Jmp", m_exmode_str[m_exmode.get()], m_move_str[m_param.get()]);

		if(m_with_turn){
			s += std::format("\n+{}旋回", m_move_str[m_turn.get()]);
		}
		return s;
	}

	virtual void GetBin(CChipBinary& bin) {
		if(m_with_turn) m_Id = CHIPID_JUMP_TURN;
		CChip::GetBin(bin);
		m_param.GetBin(bin);
		if(m_with_turn) m_turn.GetBin(bin);
		m_exmode.GetBin(bin);
	}

	// option
	auto& _wait()		{m_exmode	= EM_WAIT;	return *this;}
	#ifdef CHP
		auto& _turnLeft()	{m_turn	= LEFT;  m_with_turn = true; return *this;}
		auto& _turnRight()	{m_turn	= RIGHT; m_with_turn = true; return *this;}
	#endif
	
	ScaledInt<>	m_param;
	ScaledInt<>	m_turn;
	ScaledInt<>	m_exmode;

	bool m_with_turn = false;
};

static auto& put_jump_chip(int param) {
	return *g_pCurField->m_tree.AddToTree(std::make_unique<CChipJump>(param));
}

static auto& _jumpForward		(LastLocationArg) { LastLocation(); return put_jump_chip(CChip::FWD);}
static auto& _jumpBackward		(LastLocationArg) { LastLocation(); return put_jump_chip(CChip::BACK);}
static auto& _jumpLeft			(LastLocationArg) { LastLocation(); return put_jump_chip(CChip::LEFT);}
static auto& _jumpRight			(LastLocationArg) { LastLocation(); return put_jump_chip(CChip::RIGHT);}
static auto& _jumpForwardLeft	(LastLocationArg) { LastLocation(); return put_jump_chip(CChip::FL);}
static auto& _jumpForwardRight	(LastLocationArg) { LastLocation(); return put_jump_chip(CChip::FR);}
static auto& _jumpBackwardLeft	(LastLocationArg) { LastLocation(); return put_jump_chip(CChip::BL);}
static auto& _jumpBackwardRight	(LastLocationArg) { LastLocation(); return put_jump_chip(CChip::BR);}
static auto& _jumpUp			(LastLocationArg) { LastLocation(); return put_jump_chip(CChip::UP);}

//////////////////////////////////////////////////////////////////////////////
// 格闘

class CChipFight : public CChip {
public:
	enum {
		LOW,
		HIGH,
		LONG,
		AUTO,
	};

	static inline const char* m_type_str[] = {
		"下段", "上段", "遠距離", "自動"
	};


	CChipFight(int param){
		m_Id	= CHIPID_FIGHT;
		m_param	= param;
		m_exmode= EM_THROUGH;
	}

	virtual ~CChipFight(){}

	virtual std::string GetLayoutText(void){
		return std::format("{}{}格闘", m_exmode_str[m_exmode.get()], m_type_str[m_param.get()]);
	}

	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_param.GetBin(bin);
		m_exmode.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		m_param.SetBin(bin);
		m_exmode.SetBin(bin);
	}

	// option
	auto& _wait()		{m_exmode	= EM_WAIT;	return *this;}

	ScaledInt<> m_param;
	ScaledInt<> m_exmode;
};

static auto& put_fight_chip(int param){
	return *g_pCurField->m_tree.AddToTree(std::make_unique<CChipFight>(param));
}

static auto& _fightLow(LastLocationArg)		{ LastLocation(); return put_fight_chip(CChipFight::LOW); }
static auto& _fightHigh(LastLocationArg)	{ LastLocation(); return put_fight_chip(CChipFight::HIGH); }
static auto& _fightLong(LastLocationArg)	{ LastLocation(); return put_fight_chip(CChipFight::LONG); }
static auto& _fight(LastLocationArg)		{ LastLocation(); return put_fight_chip(CChipFight::AUTO); }

//////////////////////////////////////////////////////////////////////////////
// ガード

class CChipGuard : public CChip {
public:
	enum {
		GUARD,
		CROUCH,
	};

	static inline const char* m_type_str[] = {
		"ガード", "伏せ"
	};

	CChipGuard(int param, int num){
		m_Id	= CHIPID_GUARD;
		m_param	= param;
		m_num	= num;
		m_exmode= EM_THROUGH;
	}

	virtual ~CChipGuard(){}

	virtual std::string GetLayoutText(void){
		return std::format("{}{}\n{}/30s", m_exmode_str[m_exmode.get()], m_type_str[m_param.get()], m_num.get());
	}

	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_param.GetBin(bin);
		m_num.GetBin(bin);
		m_exmode.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		m_param.SetBin(bin);
		m_num.SetBin(bin);
		m_exmode.SetBin(bin);
	}

	// option
	auto& _wait()		{m_exmode	= EM_WAIT;	return *this;}

	ScaledInt<>			m_param;
	ScaledInt<8, 5, 60>	m_num;
	ScaledInt<>			m_exmode;
};

static auto& put_guard_chip(int param, int num){
	return *g_pCurField->m_tree.AddToTree(std::make_unique<CChipGuard>(param, num));
}

static auto& guard(int num, LastLocationArg)  { LastLocation(); return put_guard_chip(CChipGuard::GUARD, num); }
static auto& crouch(int num, LastLocationArg) { LastLocation(); return put_guard_chip(CChipGuard::CROUCH, num); }

//////////////////////////////////////////////////////////////////////////////
// スペシャル

class CChipSpecial : public CChip {
public:
	CChipSpecial(int param){
		m_Id	= CHIPID_SPECIAL;
		m_param	= param;
		m_exmode= EM_THROUGH;
	}

	virtual ~CChipSpecial(){}

	virtual std::string GetLayoutText(void){
		return std::format("{}特殊{}", m_exmode_str[m_exmode.get()], m_param.get());
	}

	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_param.GetBin(bin);
		m_exmode.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		m_param.SetBin(bin);
		m_exmode.SetBin(bin);
	}

	// option
	auto& _wait()		{m_exmode	= EM_WAIT;	return *this;}

	ScaledInt<8, 1, 3, 1, 1>	m_param;
	ScaledInt<>					m_exmode;
};

static auto& special(int param, LastLocationArg) { LastLocation(); return *g_pCurField->m_tree.AddToTree(std::make_unique<CChipSpecial>(param)); }

//////////////////////////////////////////////////////////////////////////////
// alt

class CChipAlt : public CChip {
public:
	enum {
		ASCEND,
		DESCEND,
		SET_ALT,
	};
	
	CChipAlt(int op, UINT param = 20){
		m_Id	= CHIPID_ALTITUDE;
		m_op	= op;
		m_param	= param;
	}

	virtual ~CChipAlt(){}

	virtual std::string GetLayoutText(void){
		return
			m_op.get() == ASCEND  ? "上昇+10m" :
			m_op.get() == DESCEND ? "下降-10m" :
			std::format("高度 {}m", m_param.get());
	}

	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_param.GetBin(bin);
		if(!g_bChp) m_op.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		m_param.SetBin(bin);
		if(!g_bChp) m_op.SetBin(bin);
	}

	ScaledInt<> 			m_op;
	ScaledInt<8, 20, 100>	m_param;
};

static void _ascend(LastLocationArg){LastLocation();
	g_pCurField->m_tree.AddToTree(std::make_unique<CChipAlt>(CChipAlt::ASCEND));
}

static void _descend(LastLocationArg){LastLocation();
	g_pCurField->m_tree.AddToTree(std::make_unique<CChipAlt>(CChipAlt::DESCEND));
}

static void setAltitude(int param, LastLocationArg){LastLocation();
	g_pCurField->m_tree.AddToTree(std::make_unique<CChipAlt>(CChipAlt::SET_ALT, param));
}

//////////////////////////////////////////////////////////////////////////////
// fire

class CChipFire : public CChip {
public:
	#include "opt_coordinate.h"

	enum {
		NORMAL,
		TGT,
		MOVE,
		JUMP
	};

	CChipFire(
		int weapon,
		int cnt
	){
		m_Id			= CHIPID_FIRE_NEAREST;
		m_weapon		= weapon;
		m_cnt			= cnt;
		m_firemode		= FM_NORMAL;
		m_exmode		= EM_THROUGH;

		InitCoordinate();
	}

	virtual ~CChipFire(){}

	virtual std::string GetLayoutText(void){
		if(m_mode != NORMAL){
			std::string s = std::format(
				"{}TGT射撃\n#{}x{}",
				m_exmode_str[m_exmode.get()],
				m_weapon.get(),
				m_cnt.get()
			);

			if(m_mode == MOVE){
				s += std::format("\n+{}移動", m_move_str[m_movedir.get()]);
			}else if(m_mode == JUMP){
				s += std::format("\n+{}Jmp", m_move_str[m_movedir.get()]);
			}

			return s;
		}

		return std::format(
			"{}射撃 w{}x{}\n{}",
			m_exmode_str[m_exmode.get()],
			m_weapon.get(),
			m_cnt.get(),
			GetCoordinateText()
		);
	}

	virtual void GetBin(CChipBinary& bin){
		if(     m_mode == TGT ) m_Id = CHIPID_FIRE_TGT;
		else if(m_mode == MOVE) m_Id = CHIPID_FIRE_MOVE;
		else if(m_mode == JUMP) m_Id = CHIPID_FIRE_JMP;

		CChip::GetBin(bin);

		if(m_mode == NORMAL) GetCoordinateBin(bin);
		#ifdef CHP
	
			if(m_mode >= MOVE){
				m_movedir.GetBin(bin);
			}
	
			m_weapon.GetBin(bin);	if(m_mode != NORMAL) bin.m_pos += 5;
			m_cnt.GetBin(bin);		if(m_mode != NORMAL) bin.m_pos += 3;
			
			if(m_mode <= TGT){
				m_firemode.GetBin(bin);
				if(m_mode == TGT) bin.m_pos += 4;
			}
		#else
			m_weapon.GetBin(bin);	if(m_mode != NORMAL) bin.m_pos += 5;
			
			if(m_mode == NORMAL){
				m_cnt.GetBin(bin);
				m_firemode.GetBin(bin);
			}else{
				m_firemode.GetBin(bin);	if(m_mode == TGT) bin.m_pos += 4;
				m_cnt.GetBin(bin);		if(m_mode != NORMAL) bin.m_pos += 3;
			}
		#endif
		m_exmode.GetBin(bin);
	}

	// option
	auto& _wait()			{m_exmode	= EM_WAIT;	return *this;}
	auto& _wide()			{m_firemode	= FM_WIDE;	return *this;}
	auto& _snipe()			{m_firemode	= FM_SNIPE;	return *this;}
	auto& _target()			{m_mode	= TGT;	return *this;}
	#ifdef CHP
		auto& _moveLeft()		{m_movedir	= LEFT;		m_mode = MOVE; return *this;}
		auto& _moveRight()		{m_movedir	= RIGHT;	m_mode = MOVE; return *this;}
		auto& _moveForward()	{m_movedir	= FWD;		m_mode = MOVE; return *this;}
		auto& _moveBackward()	{m_movedir	= BACK; 	m_mode = MOVE; return *this;}
		auto& _jumpLeft()		{m_movedir	= LEFT; 	m_mode = JUMP; return *this;}
		auto& _jumpRight()		{m_movedir	= RIGHT;	m_mode = JUMP; return *this;}
		auto& _jumpForward()	{m_movedir	= FWD;		m_mode = JUMP; return *this;}
		auto& _jumpBackward()	{m_movedir	= BACK; 	m_mode = JUMP; return *this;}
	#endif
	
	#ifdef CHP
		ScaledInt<3, 1, 5, 1, 1>	m_weapon;
	#else
		ScaledInt<3, 0, 5>			m_weapon;
	#endif
	ScaledInt<5, 1, 16>			m_cnt;
	ScaledInt<4>				m_firemode;
	ScaledInt<4>				m_exmode;
	ScaledInt<>					m_movedir;

	int m_mode = NORMAL;
};

static auto& fire(
	int weapon,
	int cnt = 1,
	LastLocationArg
){
	LastLocation();
	return *g_pCurField->m_tree.AddToTree(std::make_unique<CChipFire>(
		weapon,
		cnt
	));
}

//////////////////////////////////////////////////////////////////////////////
// 方位射撃

class CChipFireFixedDir : public CChip {
public:
	CChipFireFixedDir(
		int dir,
		int elev,
		int weapon,
		int cnt
	){
		m_Id			= CHIPID_FIRE_FIXED_DIR;
		m_dir			= (uint8_t)CChipFire::int2angle(dir);
		m_elev			= (uint8_t)int2elev(elev);
		m_weapon		= weapon;
		m_cnt			= cnt;
		m_exmode		= EM_THROUGH;
	}

	static int int2elev(int val){
		if (val < -90 || 90 < val) Error(std::format("Value {} is out of range [-90 - 90]", val));
		return val / 2;
	}

	static int elev2int(int val){
		return (int32_t)(int8_t)val * 2;
	}

	virtual ~CChipFireFixedDir(){}

	virtual std::string GetLayoutText(void){
		return std::format(
			"{}射撃 #{}x{}\n方位={}\n仰角={}",
			m_exmode_str[m_exmode.get()],
			m_weapon.get(),
			m_cnt.get(),
			CChipFire::angle2int(m_dir.get()),
			elev2int(m_elev.get())
		);
	}

	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_dir.GetBin(bin);
		m_elev.GetBin(bin);
		m_weapon.GetBin(bin);
		m_cnt.GetBin(bin);
		m_exmode.GetBin(bin);
	}

	// option
	auto& _wait()	{m_exmode	= EM_WAIT;	return *this;}

	ScaledInt<>					m_elev;
	ScaledInt<>					m_dir;
	#ifdef CHP
		ScaledInt<8, 1, 5, 1, 1>	m_weapon;
	#else
		ScaledInt<8, 0, 5>			m_weapon;
	#endif
	ScaledInt<8, 1, 16>			m_cnt;
	ScaledInt<>					m_exmode;

	int m_target = 0;
};

static auto& fire(
	int dir,
	int elev,
	int weapon,
	int cnt = 1,
	LastLocationArg
){
	LastLocation();
	return *g_pCurField->m_tree.AddToTree(std::make_unique<CChipFireFixedDir>(
		dir, elev, weapon, cnt
	));
}

//////////////////////////////////////////////////////////////////////////////
// カウンタ方位射撃

#ifdef CHP
class CChipFireCounterDir : public CChip {
public:
	CChipFireCounterDir(
		int dir,
		int elev,
		int weapon,
		int cnt
	){
		m_Id			= CHIPID_FIRE_COUNTER;
		m_dir			= dir;
		m_elev			= elev;
		m_weapon		= weapon;
		m_cnt			= cnt;
		m_exmode		= EM_THROUGH;
	}

	virtual ~CChipFireCounterDir(){}

	virtual std::string GetLayoutText(void){
		return std::format(
			"{}射撃 #{}x{}\n方位={}\n仰角={}",
			m_exmode_str[m_exmode.get()],
			m_weapon.get(),
			m_cnt.get(),
			m_VarNameStr[m_dir.get()],
			m_VarNameStr[m_elev.get()]
		);
	}

	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_dir.GetBin(bin);
		m_elev.GetBin(bin);
		m_weapon.GetBin(bin);
		m_cnt.GetBin(bin);
		m_exmode.GetBin(bin);
	}

	// option
	auto& _wait()	{m_exmode	= EM_WAIT;	return *this;}

	ScaledInt<>					m_elev;
	ScaledInt<>					m_dir;
	ScaledInt<8, 1, 5, 1, 1>	m_weapon;
	ScaledInt<8, 1, 16>			m_cnt;
	ScaledInt<>					m_exmode;

	int m_target = 0;
};

static auto& fire(
	CChipVar& dir,
	CChipVar& elev,
	int weapon,
	int cnt = 1,
	LastLocationArg
){
	LastLocation();
	return *g_pCurField->m_tree.AddToTree(std::make_unique<CChipFireCounterDir>(
		dir.m_var, elev.m_var, weapon, cnt
	));
}
#endif

//////////////////////////////////////////////////////////////////////////////
// option

class CChipOption : public CChip {
public:
	CChipOption(UINT param){
		m_Id	= CHIPID_OPTION;
		m_param	= param;
	}

	virtual ~CChipOption(){}

	virtual std::string GetLayoutText(void){
		return std::format("Option {}", m_param.get());
	}

	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_param.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		m_param.SetBin(bin);
	}

	ScaledInt<8, 1, 5, 1, 1> m_param;
};

static void option(UINT param, LastLocationArg){
	LastLocation();
	g_pCurField->m_tree.AddToTree(std::make_unique<CChipOption>(param));
}

//////////////////////////////////////////////////////////////////////////////
// 残弾

class CChipAmmoNum : public CChipCond {
public:

	CChipAmmoNum(
		int weapon,
		int opr		= OP_GE,
		int num		= 1
	){
		m_weapon	= IF_CHP(weapon - 1, weapon);
		m_operator	= opr;
		m_num		= num;
		m_Id		= CHIPID_IF_AMMO_NUM;
	}

	virtual ~CChipAmmoNum(){}

	virtual void set_num(int num){m_num = num;}
	virtual void set_operator(int opr){m_operator = opr;}

	virtual std::string GetLayoutText(void){
		int w = m_weapon.get();
		
		if(!g_bChp){
			if(w == 0){
				return std::format(
					"起動武装\n残数{}{}?",
					m_operator_str[m_operator.get()], m_num.get()
				);
			}
			--w;
		}
		
		return std::format(
			"{}#{}\n残数{}{}?",
			(w < 5 ? "武装" : "Opt."), (w % 5 + 1),
			m_operator_str[m_operator.get()], m_num.get()
		);
	}

	virtual void GetBin(CChipBinary& bin){
		CChipCond::GetBin(bin);
		m_num.GetBin(bin);
		m_weapon.GetBin(bin);
		m_operator.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChipCond::SetBin(bin);
		m_num.SetBin(bin);
		m_weapon.SetBin(bin);
		m_operator.SetBin(bin);
	}

	ScaledInt<16, 1, 990>	m_num;
	ScaledInt<8>			m_weapon;
	ScaledInt<8>			m_operator;
};

//////////////////////////////////////////////////////////////////////////////
// 近くの OKE を探索

class CChipOkeNum : public CChipCond {
public:
	#include "opt_coordinate.h"

	CChipOkeNum(
		int enemy
	){
		m_type		= OKE_ALL - OKE_BIPED;
		m_enemy		= enemy;
		m_Id		= CHIPID_IF_OKE_NUM;
		m_num		= 1;
		InitCoordinate();
	}

	virtual ~CChipOkeNum(){}

	virtual void set_num(int num){m_num = num;}
	virtual void set_operator(int opr){m_operator = opr;}

	virtual std::string GetLayoutText(void){
		return
			std::format(
				"{}{}\n{}{}?\n{}",
				m_EnemyFriendlyStr[m_enemy.get()],
				m_OkeTypeStr[m_type.get()], m_operator_str[m_operator.get()], m_num.get(),
				GetCoordinateText()
			);
	}

	virtual void GetBin(CChipBinary& bin){
		CChipCond::GetBin(bin);
		GetCoordinateBin(bin);
		m_enemy.GetBin(bin);
		m_type.GetBin(bin);
		m_num.GetBin(bin);
		m_operator.GetBin(bin);
	}

	// option
	auto& type(int param){
		if(param > (OKE_ALL - OKE_BIPED)) param -= OKE_BIPED;

		m_type = param;
		return *this;
	}

	ScaledInt<4>		m_enemy;
	ScaledInt<4>		m_type;
	ScaledInt<4, 1, 31>	m_num;
	ScaledInt<4>		m_operator;
};

static auto& numOke(
	int enemy
){
	return *g_pCurField->m_tree.AddToPool(std::make_unique<CChipOkeNum>(enemy));
}

static auto& _numEnemy		(LastLocationArg){LastLocation(); return numOke(CChip::ENEMY); }
static auto& _numFriendly	(LastLocationArg){LastLocation(); return numOke(CChip::FRIENDLY); }
static auto& _numAllOke		(LastLocationArg){LastLocation(); return numOke(CChip::ALL_OKE); }
#ifndef CHP
static auto& _numUnknownOke	(LastLocationArg){LastLocation(); return numOke(CChip::UNKNOWN_OKE); }
#endif

//////////////////////////////////////////////////////////////////////////////
// エリア外判定

class CChipOutsideArea : public CChipCond {
public:
	#include "opt_coordinate.h"

	CChipOutsideArea(){
		m_Id		= CHIPID_IF_OUTSIDE_AREA;
		InitCoordinate();
	}

	virtual ~CChipOutsideArea(){}

	virtual std::string GetLayoutText(void){
		return
			std::format(
				"エリア外?\n{}",
				GetCoordinateText()
			);
	}

	virtual void GetBin(CChipBinary& bin){
		CChipCond::GetBin(bin);
		GetCoordinateBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChipCond::SetBin(bin);
		SetCoordinateBin(bin);
	}
};

static auto& _isOutsideArea(
	LastLocationArg
){
	LastLocation();
	return *g_pCurField->m_tree.AddToPool(std::make_unique<CChipOutsideArea>());
}

//////////////////////////////////////////////////////////////////////////////
// 近くの障害物を探索

class CChipBarrier : public CChipCond {
public:
	#include "opt_coordinate.h"

	CChipBarrier(
		int num,
		int opr		= OP_GE
	){
		m_barrier_height	= num;
		m_operator	= opr;
		m_Id		= CHIPID_IF_BARRIER;
		InitCoordinate();
	}

	virtual ~CChipBarrier(){}

	virtual void set_num(int num){m_barrier_height = num;}
	virtual void set_operator(int opr){m_operator = opr;}

	virtual std::string GetLayoutText(void){
		return
			std::format(
				"障害物\n高さ{}{}m?\n{}",
				m_operator_str[m_operator.get()], m_barrier_height.get(),
				GetCoordinateText()
			);
	}

	virtual void GetBin(CChipBinary& bin){
		CChipCond::GetBin(bin);
		GetCoordinateBin(bin);
		m_barrier_height.GetBin(bin);
		m_operator.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChipCond::SetBin(bin);
		SetCoordinateBin(bin);
		m_barrier_height.SetBin(bin);
		m_operator.SetBin(bin);
	}

	ScaledInt<8, 0, 30>	m_barrier_height;
	ScaledInt<8>		m_operator;
};

static auto& isBarrierOver(
	int num,
	LastLocationArg
){
	LastLocation();
	return *g_pCurField->m_tree.AddToPool(std::make_unique<CChipBarrier>(num, CChip::OP_GE));
}

static auto& isBarrierUnder(
	int num,
	LastLocationArg
){
	LastLocation();
	return *g_pCurField->m_tree.AddToPool(std::make_unique<CChipBarrier>(num, CChip::OP_LE));
}

//////////////////////////////////////////////////////////////////////////////
// 近くの危険物を探索

class CChipProjectileNum : public CChipCond {
public:
	#include "opt_coordinate.h"

	static inline const char *m_ProjectileTypeStr[] = {
		"徹甲弾", "ビーム", "パルス", "ナパーム", "グレネード", "爆弾", "ロケット", "ミサイル", "地雷", "機雷", "高速", "全種"
	};

	CChipProjectileNum(
	){
		m_type		= P_ALL;
		m_Id		= CHIPID_IF_DET_PROJECTILE;
		m_num		= 1;
		InitCoordinate();
	}

	virtual ~CChipProjectileNum(){}

	virtual void set_num(int num){m_num = num;}
	virtual void set_operator(int opr){m_operator = opr;}

	virtual std::string GetLayoutText(void){
		return
			std::format(
				"{}\n{}{}?\n{}",
				m_ProjectileTypeStr[m_type.get()], m_operator_str[m_operator.get()], m_num.get(),
				GetCoordinateText()
			);
	}

	virtual void GetBin(CChipBinary& bin){
		CChipCond::GetBin(bin);
		GetCoordinateBin(bin);
		m_type.GetBin(bin);
		m_num.GetBin(bin);
		m_operator.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChipCond::SetBin(bin);
		SetCoordinateBin(bin);
		m_type.SetBin(bin);
		m_num.SetBin(bin);
		m_operator.SetBin(bin);
	}

	// option
	auto& type(int param)	{m_type = param;	return *this;}

	ScaledInt<8>		m_type;
	ScaledInt<4, 1, 31>	m_num;
	ScaledInt<4>		m_operator;
};

static auto& _numProjectile(
	LastLocationArg
){
	LastLocation();
	return *g_pCurField->m_tree.AddToPool(std::make_unique<CChipProjectileNum>());
}

//////////////////////////////////////////////////////////////////////////////
// 自機の状態確認

class CChipSelfStatus : public CChipCond {
public:
	enum {
		HP,
		ENERGY,
		HEAT,
	};

	CChipSelfStatus(
		int type
	){
		m_type			= type;
		m_operator		= OP_GE;
		m_num			= 1;
		m_Id			= CHIPID_IF_SELF_STATUS;
	}

	virtual ~CChipSelfStatus(){}

	virtual void set_num(int num){m_num = num;}
	virtual void set_operator(int opr){m_operator = opr;}

	virtual std::string GetLayoutText(void){
		return
			std::format(
				"{}{}{}?",
				m_SelfStatusTypeStr[m_type.get()],
				m_operator_str[m_operator.get()], m_num.get()
			);
	}

	virtual void GetBin(CChipBinary& bin){
		CChipCond::GetBin(bin);
		m_type.GetBin(bin);
		m_num.GetBin(bin);
		m_operator.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChipCond::SetBin(bin);
		m_type.SetBin(bin);
		m_num.SetBin(bin);
		m_operator.SetBin(bin);
	}

	ScaledInt<>		m_type;
	ScaledInt<>		m_num;
	ScaledInt<>		m_operator;
};

//////////////////////////////////////////////////////////////////////////////
// SUB1 / 2

class CChipSubroutine : public CChip {
public:
	CChipSubroutine(
		int no
	){
		m_Id	= CHIPID_SUB;
		m_no	= no;
	}

	virtual ~CChipSubroutine(){}

	virtual std::string GetLayoutText(void){
		return std::format("SUB{}", m_no.get());
	}

	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_no.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		m_no.SetBin(bin);
	}

	ScaledInt<8, 1, 2, 1, 1> m_no;
};

static void put_sub_chip(
	int no,
	LastLocationArg
){
	LastLocation();
	g_pCurField->m_tree.AddToTree(std::make_unique<CChipSubroutine>(no));
}

//////////////////////////////////////////////////////////////////////////////
// 乱数

class CChipIsRand : public CChipCond {
public:

	CChipIsRand(
		int num
	){
		m_Id	= CHIPID_IF_RAND;
		m_num	= num;
	}

	virtual ~CChipIsRand(){}

	virtual std::string GetLayoutText(void){
		return
			std::format(
				"乱数\n{}%?",
				m_num.get()
			);
	}

	virtual void GetBin(CChipBinary& bin){
		CChipCond::GetBin(bin);
		m_num.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChipCond::SetBin(bin);
		m_num.SetBin(bin);
	}

	ScaledInt<8, 1, 99>		m_num;
};

static auto& isRand(
	int num,
	LastLocationArg
){
	LastLocation();
	return *g_pCurField->m_tree.AddToPool(std::make_unique<CChipIsRand>(num));
}

//////////////////////////////////////////////////////////////////////////////
// 時間

class CChipIfTime : public CChipCond {
public:

	enum{
		START,
		END,
	};

	CChipIfTime(int start_end){
		m_operator		= OP_GE;
		m_start_end		= start_end;
		m_num			= 1;
		m_Id			= CHIPID_IF_TIME;
	}

	virtual ~CChipIfTime(){}

	virtual void set_num(int num){m_num = num;}
	virtual void set_operator(int opr){m_operator = opr;}

	virtual std::string GetLayoutText(void){
		return
			std::format(
				"{}時間\n{}{}?",
				m_start_end.get() ? "残" : "経過",
				m_operator_str[m_operator.get()], m_num.get()
			);
	}

	virtual void GetBin(CChipBinary& bin){
		CChipCond::GetBin(bin);
		m_num.GetBin(bin);
		m_start_end.GetBin(bin);
		m_operator.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChipCond::SetBin(bin);
		m_num.SetBin(bin);
		m_start_end.SetBin(bin);
		m_operator.SetBin(bin);
	}

	ScaledInt<16, 1, 300>	m_num;
	ScaledInt<>				m_start_end;
	ScaledInt<>				m_operator;
};

//////////////////////////////////////////////////////////////////////////////
// ターゲット機体コード

class CChipIfBodyCode : public CChipCond {
public:
	CChipIfBodyCode(){
		m_Id			= CHIPID_TGT_BODYCODE;
		m_cond_eq		= true;
	}

	virtual ~CChipIfBodyCode(){}

	virtual void set_num(int num){m_bodycode = num;}

	virtual std::string GetLayoutText(void){
		return
			std::format("機体コード\n={}?", m_bodycode.m_value == 0xFF ? 0 : m_bodycode.get());
	}

	virtual void GetBin(CChipBinary& bin){
		CChipCond::GetBin(bin);
		m_bodycode.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChipCond::SetBin(bin);
		m_bodycode.SetBin(bin);
	}

	ScaledInt<8, 0, IF_CHP(37, 42), 1, 1>	m_bodycode;
};

//////////////////////////////////////////////////////////////////////////////
// 被ロック数

class CChipIfNumLock : public CChipCond {
public:
	CChipIfNumLock(){
		m_Id			= CHIPID_NUM_LOCKED;
		m_cond_eq		= true;
	}

	virtual ~CChipIfNumLock(){}

	virtual void set_num(int num){m_num = num;}

	virtual std::string GetLayoutText(void){
		return
			std::format("ロック数={}?", m_num.get());
	}

	virtual void GetBin(CChipBinary& bin){
		CChipCond::GetBin(bin);
		m_num.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChipCond::SetBin(bin);
		m_num.SetBin(bin);
	}

	ScaledInt<8, 1, 3>	m_num;
};

static auto _numLocked(LastLocationArg) {
	LastLocation();
	return *g_pCurField->m_tree.AddToPool(std::make_unique<CChipIfNumLock>());
}

//////////////////////////////////////////////////////////////////////////////
// 武装 ID

class CChipIfTargetWeaponId : public CChipCond {
public:
	CChipIfTargetWeaponId(int weapon){
		m_Id			= CHIPID_WEAPON_ID;
		m_weapon		= weapon;
		m_cond_eq		= true;
	}

	static inline const char *m_TypeStr[] = {
		"武装なし",
		"アサルト",
		"ビーム",
		"パルス",
		"ナパーム",
		"炸裂砲",
		"ショットガン",
		"カノン",
		"レールガン",
		"グレネード",
		"爆弾",
		"ロケット",
		"ミサイル",
		"地雷",
		"機雷",
	};

	virtual ~CChipIfTargetWeaponId(){}

	virtual void set_num(int num){m_num = num;}

	virtual std::string GetLayoutText(void){
		return
			std::format("敵武装{}=\n{}?", m_weapon.get(), m_TypeStr[m_num.get()]);
	}

	virtual void GetBin(CChipBinary& bin){
		CChipCond::GetBin(bin);
		m_weapon.GetBin(bin);
		m_num.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChipCond::SetBin(bin);
		m_weapon.SetBin(bin);
		m_num.SetBin(bin);
	}

	ScaledInt<8, 1, 5, 1, 1>	m_weapon;
	ScaledInt<>					m_num;
};

static auto targetWeaponId(int weapon, LastLocationArg) {
	LastLocation();
	return *g_pCurField->m_tree.AddToPool(std::make_unique<CChipIfTargetWeaponId>(weapon));
}

//////////////////////////////////////////////////////////////////////////////
// 射線クリア?

class CChipIfLineBlocked : public CChipCond {
public:
	enum {
		TERRAIN,
		FRIENDLY,
		BARRIER,
		ANY,
	};
	
	static inline const char *m_TypeStr[] = {
		"地表面", "味方", "障害物", "何れか"
	};
	
	CChipIfLineBlocked(int param){
		m_Id		= CHIPID_IS_LINE_CLEAR;
		m_param		= param;
	}

	virtual ~CChipIfLineBlocked(){}

	virtual std::string GetLayoutText(void){
		return std::format("{}\nにより\n射線妨害?", m_TypeStr[m_param.get()]);
	}

	virtual void GetBin(CChipBinary& bin){
		CChipCond::GetBin(bin);
		m_param.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChipCond::SetBin(bin);
		m_param.SetBin(bin);
	}
	
	ScaledInt<> m_param;
};

static auto _isLineBlocked(LastLocationArg) {
	LastLocation();
	return *g_pCurField->m_tree.AddToPool(std::make_unique<CChipIfLineBlocked>(IF_CHP(CChipIfLineBlocked::TERRAIN, CChipIfLineBlocked::ANY)));
}

static auto _isLineBlockedTerrain(LastLocationArg) {
	LastLocation();
	return *g_pCurField->m_tree.AddToPool(std::make_unique<CChipIfLineBlocked>(CChipIfLineBlocked::TERRAIN));
}

static auto _isLineBlockedFriendly(LastLocationArg) {
	LastLocation();
	return *g_pCurField->m_tree.AddToPool(std::make_unique<CChipIfLineBlocked>(CChipIfLineBlocked::FRIENDLY));
}

static auto _isLineBlockedBarrier(LastLocationArg) {
	LastLocation();
	return *g_pCurField->m_tree.AddToPool(std::make_unique<CChipIfLineBlocked>(CChipIfLineBlocked::BARRIER));
}

//////////////////////////////////////////////////////////////////////////////
// ターゲット指定

class CChipLockon : public CChip {
public:
	#include "opt_coordinate.h"
	
	enum {
		NEAREST,
		FARTHEST,
		FRONT,
	};
	
	static inline const char *m_PriorityStr[] = {
		"最近", "最遠", "正面"
	};
	
	
	CChipLockon(
		int enemy
	){
		m_type		= OKE_TGT_ALL - OKE_BIPED;
		m_enemy		= enemy;
		m_Id		= CHIPID_LOCKON;

		InitCoordinate();
	}

	virtual ~CChipLockon(){}

	virtual std::string GetLayoutText(void){
		return
			std::format(
				"Lock{}\n{}{}\n{}",
				m_EnemyFriendlyStr[m_enemy.get()],
				#ifdef CHP
					"",
				#else
					m_PriorityStr[m_priority.get()],
				#endif
				m_EnemyFriendlyStr[
					m_enemy.get() == OKE_TGT_ALL ? OKE_ALL :
					m_enemy.get() == OKE_TGT_GROUND ? OKE_GROUND :
					m_enemy.get()
				],
				GetCoordinateText()
			);
	}

	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		GetCoordinateBin(bin);
		#ifndef CHP
			m_priority.GetBin(bin);
		#endif
		m_enemy.GetBin(bin);
		m_type.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		SetCoordinateBin(bin);
		#ifndef CHP
			m_priority.SetBin(bin);
		#endif
		m_enemy.SetBin(bin);
		m_type.SetBin(bin);
	}

	// option
	auto& type(int param){
		m_type =	param == OKE_ALL	? OKE_TGT_ALL :
					param == OKE_GROUND	? OKE_TGT_GROUND :
					param >= OKE_BIPED	? param - OKE_BIPED :
					param;
		return *this;
	}
	
	#ifndef CHP
		auto& _farthest(){m_priority = FARTHEST; return *this;}
		auto& _front  (){m_priority = FRONT;   return *this;}
	#endif

	#ifdef CHP
		ScaledInt<>		m_enemy;
	#else
		ScaledInt<4>	m_priority;
		ScaledInt<4>	m_enemy;
	#endif
	ScaledInt<>		m_type;
};

static auto& _lockon(LastLocationArg){
	LastLocation();
	return *g_pCurField->m_tree.AddToTree(std::make_unique<CChipLockon>(CChip::ENEMY));
}

static auto& _lockonFriendly(LastLocationArg) {
	LastLocation();
	return *g_pCurField->m_tree.AddToTree(std::make_unique<CChipLockon>(CChip::FRIENDLY));
}

static auto& _lockonAll(LastLocationArg) {
	LastLocation();
	return *g_pCurField->m_tree.AddToTree(std::make_unique<CChipLockon>(CChip::ALL_OKE));
}

class CChipUnlock : public CChip {
	public:
	CChipUnlock(){
		m_Id = CHIPID_LOCK_RELEASE;
	}
	virtual ~CChipUnlock(){}
	virtual std::string GetLayoutText(void){
		return "Unlock";
	}
};

static void _unlock(
	LastLocationArg
){
	LastLocation();
	g_pCurField->m_tree.AddToTree(std::make_unique<CChipUnlock>());
}

//////////////////////////////////////////////////////////////////////////////
// ターゲット指定

class CChipLockonID : public CChip {
public:
	CChipLockonID(
		int var
	){
		m_var		= var;
		m_Id		= CHIPID_LOCKON_COUNTER;
	}

	virtual ~CChipLockonID(){}

	virtual std::string GetLayoutText(void){
		return
			std::format(
				"Lock\nId={}", m_VarNameStr[m_var.get()]
			);
	}

	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_var.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		m_var.SetBin(bin);
	}

	ScaledInt<>		m_var;
};

static auto& lockonId(CChipVar& id, LastLocationArg){
	LastLocation();
	return *g_pCurField->m_tree.AddToTree(std::make_unique<CChipLockonID>(id.m_var));
}

//////////////////////////////////////////////////////////////////////////////
// ターゲット位置

class CChipTgtPosition : public CChipCond {
public:
	#include "opt_coordinate.h"

	enum {
		TGT,
		FROM_TGT,
	};

	static inline const char* m_tgt_str[] = {
		"", "からの\n",
	};

	CChipTgtPosition(
		int tgt
	){
		m_Id	= tgt == TGT ? CHIPID_IF_TGT_POS : CHIPID_IF_POS_FROM_TGT;
		InitCoordinate();
	}


	virtual ~CChipTgtPosition(){}

	virtual std::string GetLayoutText(void){
		return
			std::format(
				"TGT{}位置?\n{}",
				m_tgt_str[m_Id.get() == CHIPID_IF_TGT_POS ? 0 : 1], GetCoordinateText()
			);
	}

	virtual void GetBin(CChipBinary& bin){
		CChipCond::GetBin(bin);
		GetCoordinateBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChipCond::SetBin(bin);
		SetCoordinateBin(bin);
	}
};

static auto& _isTargetPosition(
	LastLocationArg
){
	LastLocation();
	return *g_pCurField->m_tree.AddToPool(std::make_unique<CChipTgtPosition>(CChipTgtPosition::TGT));
}

static auto& _isPositionFromTarget(
	LastLocationArg
){
	LastLocation();
	return *g_pCurField->m_tree.AddToPool(std::make_unique<CChipTgtPosition>(CChipTgtPosition::FROM_TGT));
}

//////////////////////////////////////////////////////////////////////////////
// ターゲット状態

class CChipTgtAction : public CChipCond {
public:

	enum {
		WAIT,
		MOVE,
		TURN,
		JUMP,
		FIRE,
		FIGHT,
		DEFENCE,
		SPECIAL,
		STUN,
		UNLOCK,
		OPTION1_ACTIVE,
		OPTION2_ACTIVE,
		OPTION3_ACTIVE,
		OPTION4_ACTIVE,
		GUNSIGHT_ACTIVE,
		AUTOTURN_ACTIVE,
		WEAPON1_ACTIVE,
		WEAPON2_ACTIVE,
		WEAPON3_ACTIVE,
		WEAPON4_ACTIVE,
		WEAPON5_ACTIVE,
	};
	
	enum {
		SHIELD = UNLOCK,
		OVERHEAT,
		MUTUAL_LOCK,
	};

	static inline const char *m_StatusTypeStr[] = {
		"静止", "移動", "旋回", "Jmp", "射撃", "格闘", "防御", "特殊", "被弾", "Unlock",
		"Opt.1起動", "Opt.2起動", "Opt.3起動", "Opt.4起動", "Gun起動", "武装1起動", "武装2起動", "武装3起動", "武装4起動", "武装5起動"
	};
	
	static inline const char *m_StatusTypeStr2[] = {
		"シールド", "加熱", "相互ロック"
	};
	
	CChipTgtAction(
		int my_tgt,
		int param
	){
		m_param		= param;
		m_Id		= my_tgt == CChip::MY ? CHIPID_IF_MY_ACTION : CHIPID_IF_TGT_ACTION;
	}

	virtual ~CChipTgtAction(){}

	virtual std::string GetLayoutText(void){
		return
			std::format(
				"{}\n{}?",
				m_SelfTgtTypeStr[m_Id.get() == CHIPID_IF_MY_ACTION ? 0 : 1],
				m_Id.get() == CHIPID_IF_TGT_ACTION && m_param.get() >= SHIELD ?
					m_StatusTypeStr2[m_param.get() - SHIELD] :
					m_StatusTypeStr[m_param.get()]
			);
	}

	virtual void GetBin(CChipBinary& bin){
		CChipCond::GetBin(bin);
		m_param.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChipCond::SetBin(bin);
		m_param.SetBin(bin);
	}

	ScaledInt<>	m_param;
};

static auto& is_self_target_status(
	int my_tgt,
	int param
){
	return *g_pCurField->m_tree.AddToPool(std::make_unique<CChipTgtAction>(my_tgt, param));
}

#ifndef NO_OKECC_SYNTAX
static auto& _isSelfWaiting			(LastLocationArg){LastLocation(); return is_self_target_status(CChip::MY, CChipTgtAction::WAIT);}
static auto& _isSelfMoving			(LastLocationArg){LastLocation(); return is_self_target_status(CChip::MY, CChipTgtAction::MOVE);}
static auto& _isSelfTurning			(LastLocationArg){LastLocation(); return is_self_target_status(CChip::MY, CChipTgtAction::TURN);}
static auto& _isSelfJumping			(LastLocationArg){LastLocation(); return is_self_target_status(CChip::MY, CChipTgtAction::JUMP);}
static auto& _isSelfFiring			(LastLocationArg){LastLocation(); return is_self_target_status(CChip::MY, CChipTgtAction::FIRE);}
static auto& _isSelfFighting		(LastLocationArg){LastLocation(); return is_self_target_status(CChip::MY, CChipTgtAction::FIGHT);}
static auto& _isSelfGuarding		(LastLocationArg){LastLocation(); return is_self_target_status(CChip::MY, CChipTgtAction::DEFENCE);}
static auto& _isSelfSpecial			(LastLocationArg){LastLocation(); return is_self_target_status(CChip::MY, CChipTgtAction::SPECIAL);}
static auto& _isSelfStumbling		(LastLocationArg){LastLocation(); return is_self_target_status(CChip::MY, CChipTgtAction::STUN);}
static auto& _isUnlock				(LastLocationArg){LastLocation(); return is_self_target_status(CChip::MY, CChipTgtAction::UNLOCK);}

#ifndef CHP
static auto& isOptionActive(int option,	 LastLocationArg){LastLocation(); return is_self_target_status(CChip::MY, CChipTgtAction::OPTION1_ACTIVE + option - 1);}
static auto& _isGunsightActive			(LastLocationArg){LastLocation(); return is_self_target_status(CChip::MY, CChipTgtAction::GUNSIGHT_ACTIVE);}
static auto& _isAutoTurnActive			(LastLocationArg){LastLocation(); return is_self_target_status(CChip::MY, CChipTgtAction::AUTOTURN_ACTIVE);}
static auto& isWeaponActive(int weapon,	 LastLocationArg){LastLocation(); return is_self_target_status(CChip::MY, CChipTgtAction::WEAPON1_ACTIVE + weapon - 1);}
#endif

static auto& _isTargetWaiting		(LastLocationArg){LastLocation(); return is_self_target_status(CChip::TARGET, CChipTgtAction::WAIT);}
static auto& _isTargetMoving		(LastLocationArg){LastLocation(); return is_self_target_status(CChip::TARGET, CChipTgtAction::MOVE);}
static auto& _isTargetTurning		(LastLocationArg){LastLocation(); return is_self_target_status(CChip::TARGET, CChipTgtAction::TURN);}
static auto& _isTargetJumping		(LastLocationArg){LastLocation(); return is_self_target_status(CChip::TARGET, CChipTgtAction::JUMP);}
static auto& _isTargetFiring		(LastLocationArg){LastLocation(); return is_self_target_status(CChip::TARGET, CChipTgtAction::FIRE);}
static auto& _isTargetFighting		(LastLocationArg){LastLocation(); return is_self_target_status(CChip::TARGET, CChipTgtAction::FIGHT);}
static auto& _isTargetGuarding		(LastLocationArg){LastLocation(); return is_self_target_status(CChip::TARGET, CChipTgtAction::DEFENCE);}
static auto& _isTargetSpecial		(LastLocationArg){LastLocation(); return is_self_target_status(CChip::TARGET, CChipTgtAction::SPECIAL);}
static auto& _isTargetStumbling		(LastLocationArg){LastLocation(); return is_self_target_status(CChip::TARGET, CChipTgtAction::STUN);}
static auto& _isTargetSheldActive	(LastLocationArg){LastLocation(); return is_self_target_status(CChip::TARGET, CChipTgtAction::SHIELD);}
static auto& _isTargetOverheat		(LastLocationArg){LastLocation(); return is_self_target_status(CChip::TARGET, CChipTgtAction::OVERHEAT);}
static auto& _isMutualLock			(LastLocationArg){LastLocation(); return is_self_target_status(CChip::TARGET, CChipTgtAction::MUTUAL_LOCK);}
#endif

//////////////////////////////////////////////////////////////////////////////
// ターゲットに向けて自動旋回

class CChipAutoTurn : public CChip {
public:
	#include "opt_coordinate.h"

	CChipAutoTurn(){
		m_Id	= CHIPID_AUTO_TURN;
		m_on	= 1;
		InitCoordinate();
	}

	virtual ~CChipAutoTurn(){}

	virtual std::string GetLayoutText(void){
		return
			std::format(
				"自動旋回{}\n{}",
				m_on.get() ? "ON" : "OFF",
				GetCoordinateText()
			);
	}

	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		GetCoordinateBin(bin);
		m_on.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		SetCoordinateBin(bin);
		m_on.SetBin(bin);
	}

	// option
	auto& _off() { m_on = 0;		return *this; }

	ScaledInt<>		m_on;
};

static auto& _autoTurn(
	LastLocationArg
){
	LastLocation();
	return *g_pCurField->m_tree.AddToTree(std::make_unique<CChipAutoTurn>());
}

//////////////////////////////////////////////////////////////////////////////
// 照準箇所

class CChipLockonPart : public CChip {
public:
	CChipLockonPart(UINT param){
		m_Id	= CHIPID_LOCKON_PARTS;
		m_param	= param;
	}

	virtual ~CChipLockonPart(){}

	virtual std::string GetLayoutText(void){
		return m_param.get() == 0?
			"照準箇所\nボティ" :
			std::format("照準箇所\n武装{}", m_param.get());
	}

	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_param.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		m_param.SetBin(bin);
	}

	ScaledInt<> m_param;
};

static void lockonPart(int param, LastLocationArg){
	LastLocation();
	g_pCurField->m_tree.AddToTree(std::make_unique<CChipLockonPart>(param));
}

//////////////////////////////////////////////////////////////////////////////
// カウンタに状態入力

class CChipGetStatus : public CChip {
public:

	CChipGetStatus(int param, int var){
		m_var	= var;
		m_param	= param;
		m_Id	= CHIPID_GET_STATUS;
	}

	virtual ~CChipGetStatus(){}

	virtual std::string GetLayoutText(void){
		return
			std::format(
				"{}=\n{}",
				m_VarNameStr[m_var.get()],
				m_StatusTypeStr[m_param.get()]
			);
	}

	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_var.GetBin(bin);
		m_param.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		m_var.SetBin(bin);
		m_param.SetBin(bin);
	}

	ScaledInt<>			m_var;
	ScaledInt<>			m_param;
};

//////////////////////////////////////////////////////////////////////////////
// CH

class CChipChSend : public CChip {
public:
	CChipChSend(int ch, int var){
		m_var	= var;
		m_ch	= ch;
		m_Id	= CHIPID_CH_SEND;
	}

	virtual ~CChipChSend(){}

	virtual std::string GetLayoutText(void){
		return std::format(
			"送信\nCH{}≪{}",
			m_ch.get(),
			m_VarNameStr[m_var.get()]
		);
	}

	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_var.GetBin(bin);
		m_ch.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		m_var.SetBin(bin);
		m_ch.SetBin(bin);
	}

	ScaledInt<>					m_var;
	ScaledInt<8, 1, 8, 1, 1>	m_ch;
};

static void chSend(
	CChipVar& var,
	int ch,
	LastLocationArg
){
	LastLocation();
	g_pCurField->m_tree.AddToTree(std::make_unique<CChipChSend>(ch, var.m_var));
}

class CChipChReceive : public CChip {
public:
	CChipChReceive(int ch, int var){
		m_var	= var;
		m_ch	= ch;
		m_Id	= CHIPID_CH_RECV;
	}

	virtual ~CChipChReceive(){}

	virtual std::string GetLayoutText(void){
		return std::format(
			"受信\n{}≪CH{}",
			m_VarNameStr[m_var.get()],
			m_ch.get()
		);
	}

	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_var.GetBin(bin);
		m_ch.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		m_var.SetBin(bin);
		m_ch.SetBin(bin);
	}

	ScaledInt<>					m_var;
	ScaledInt<8, 1, 8, 1, 1>	m_ch;
};

//////////////////////////////////////////////////////////////////////////////
// 算術演算

class CChipCalc : public CChip {
public:
	enum {
		MOV, ADD, SUB, MUL, DIV, INT, MOD, ABS, MAX, MIN, SQR,
		SIN, COS, TAN, ATAN, NOT, AND, OR, XOR
	};

	static inline const char *m_OprStr[] = {
		"=", "+=", "-=", "*=", "/=", "= int", "%=", "= abs", "= max", "= min", "= sqr",
		"= sin", "= cos", "= tan", "= atan", "= not", "&amp;=", "|=", "^=",
	};

	CChipCalc(
		UINT op1,
		UINT opr,
		UINT op2
	){
		m_op1		= op1;
		m_operator	= opr;
		m_immxvar	= 0;
		m_op2		= op2;
		m_imm		= 0;
		m_Id		= CHIPID_CALC;
	}

	CChipCalc(
		UINT op1,
		UINT opr,
		double op2
	){
		m_op1		= op1;
		m_operator	= opr;
		m_immxvar	= 1;
		m_imm		= double2i32(op2);
		m_op2		= 0;
		m_Id		= CHIPID_CALC;
	}

	virtual ~CChipCalc(){}

	virtual std::string GetLayoutText(void){
		return m_immxvar.get() ?
			std::format("{}{}\n{}", m_VarNameStr[m_op1.get()], m_OprStr[m_operator.get()], int2fixp32s(m_imm.get())) :
			std::format("{}{}{}", m_VarNameStr[m_op1.get()], m_OprStr[m_operator.get()], m_VarNameStr[m_op2.get()]);
	}

	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_imm.GetBin(bin);
		m_op1.GetBin(bin);
		m_op2.GetBin(bin);
		m_immxvar.GetBin(bin);
		m_operator.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		m_imm.SetBin(bin);
		m_op1.SetBin(bin);
		m_op2.SetBin(bin);
		m_immxvar.GetBin(bin);
		m_operator.SetBin(bin);
	}

	ScaledInt<32>	m_imm;
	ScaledInt<4>	m_op1;
	ScaledInt<3>	m_op2;
	ScaledInt<1>	m_immxvar;
	ScaledInt<>		m_operator;
};

#ifndef NO_OKECC_SYNTAX
CChipVar& CChipVar::operator+=(const CChipVar& op2){g_pCurField->m_tree.AddToTree(std::make_unique<CChipCalc>(m_var, CChipCalc::ADD, op2.m_var)); return *this;}
CChipVar& CChipVar::operator-=(const CChipVar& op2){g_pCurField->m_tree.AddToTree(std::make_unique<CChipCalc>(m_var, CChipCalc::SUB, op2.m_var)); return *this;}
CChipVar& CChipVar::operator*=(const CChipVar& op2){g_pCurField->m_tree.AddToTree(std::make_unique<CChipCalc>(m_var, CChipCalc::MUL, op2.m_var)); return *this;}
CChipVar& CChipVar::operator/=(const CChipVar& op2){g_pCurField->m_tree.AddToTree(std::make_unique<CChipCalc>(m_var, CChipCalc::DIV, op2.m_var)); return *this;}
CChipVar& CChipVar::operator%=(const CChipVar& op2){g_pCurField->m_tree.AddToTree(std::make_unique<CChipCalc>(m_var, CChipCalc::MOD, op2.m_var)); return *this;}
CChipVar& CChipVar::operator= (const CChipVar& op2){g_pCurField->m_tree.AddToTree(std::make_unique<CChipCalc>(m_var, CChipCalc::MOV, op2.m_var)); return *this;}
CChipVar& CChipVar::operator&=(const CChipVar& op2){g_pCurField->m_tree.AddToTree(std::make_unique<CChipCalc>(m_var, CChipCalc::AND, op2.m_var)); return *this;}
CChipVar& CChipVar::operator|=(const CChipVar& op2){g_pCurField->m_tree.AddToTree(std::make_unique<CChipCalc>(m_var, CChipCalc::OR,  op2.m_var)); return *this;}
CChipVar& CChipVar::operator^=(const CChipVar& op2){g_pCurField->m_tree.AddToTree(std::make_unique<CChipCalc>(m_var, CChipCalc::XOR, op2.m_var)); return *this;}

CChipVar& CChipVar::operator+=(const double imm) { g_pCurField->m_tree.AddToTree(std::make_unique<CChipCalc>(m_var, CChipCalc::ADD, imm)); return *this; }
CChipVar& CChipVar::operator-=(const double imm) { g_pCurField->m_tree.AddToTree(std::make_unique<CChipCalc>(m_var, CChipCalc::SUB, imm)); return *this; }
CChipVar& CChipVar::operator*=(const double imm) { g_pCurField->m_tree.AddToTree(std::make_unique<CChipCalc>(m_var, CChipCalc::MUL, imm)); return *this; }
CChipVar& CChipVar::operator/=(const double imm) { g_pCurField->m_tree.AddToTree(std::make_unique<CChipCalc>(m_var, CChipCalc::DIV, imm)); return *this; }
CChipVar& CChipVar::operator%=(const double imm) { g_pCurField->m_tree.AddToTree(std::make_unique<CChipCalc>(m_var, CChipCalc::MOD, imm)); return *this; }
CChipVar& CChipVar::operator= (const double imm) { g_pCurField->m_tree.AddToTree(std::make_unique<CChipCalc>(m_var, CChipCalc::MOV, imm)); return *this; }
CChipVar& CChipVar::operator&=(const double imm) { g_pCurField->m_tree.AddToTree(std::make_unique<CChipCalc>(m_var, CChipCalc::AND, imm)); return *this; }
CChipVar& CChipVar::operator|=(const double imm) { g_pCurField->m_tree.AddToTree(std::make_unique<CChipCalc>(m_var, CChipCalc::OR,  imm)); return *this; }
CChipVar& CChipVar::operator^=(const double imm) { g_pCurField->m_tree.AddToTree(std::make_unique<CChipCalc>(m_var, CChipCalc::XOR, imm)); return *this; }

CChipVar& CChipVar::operator++(){return *this += 1.0;}
CChipVar& CChipVar::operator--(){return *this -= 1.0;}
#endif

//////////////////////////////////////////////////////////////////////////////
// val 系

static CChipVal _numAllEnemy		(LastLocationArg){LastLocation(); return CChipVal(CChipVal::ENEMY);}
static CChipVal _numAllFriendly		(LastLocationArg){LastLocation(); return CChipVal(CChipVal::FRIENDLY);}
static CChipVal _time				(LastLocationArg){LastLocation(); return CChipVal(CChipVal::TIME);}
static CChipVal _mathRand			(LastLocationArg){LastLocation(); return CChipVal(CChipVal::RAND);}

static CChipVal _myX				(LastLocationArg){LastLocation(); return CChipVal(CChipVal::MY_POS_X);}
static CChipVal _myY				(LastLocationArg){LastLocation(); return CChipVal(CChipVal::MY_POS_Y);}
static CChipVal _myZ				(LastLocationArg){LastLocation(); return CChipVal(CChipVal::MY_POS_Z);}
static CChipVal _myDirection		(LastLocationArg){LastLocation(); return CChipVal(CChipVal::MY_DIRECTION);}

static CChipVal _targetId			(LastLocationArg){LastLocation(); return CChipVal(CChipVal::TGT_NO);}
static CChipVal _targetAzimuth		(LastLocationArg){LastLocation(); return CChipVal(CChipVal::TGT_AZIMUTH);}
static CChipVal _targetElevation	(LastLocationArg){LastLocation(); return CChipVal(CChipVal::TGT_ELEVATION);}
static CChipVal _targetX			(LastLocationArg){LastLocation(); return CChipVal(CChipVal::TGT_POS_X);}
static CChipVal _targetY			(LastLocationArg){LastLocation(); return CChipVal(CChipVal::TGT_POS_Y);}
static CChipVal _targetZ			(LastLocationArg){LastLocation(); return CChipVal(CChipVal::TGT_POS_Z);}
static CChipVal _targetDirection	(LastLocationArg){LastLocation(); return CChipVal(CChipVal::TGT_DIRECTION);}
static CChipVal _targetBodyCode		(LastLocationArg){LastLocation(); return CChipVal(CChipVal::TGT_BODYCODE);}
static CChipVal _targetActCode		(LastLocationArg){LastLocation(); return CChipVal(CChipVal::TGT_ACTCODE);}
static CChipVal _targetDistance		(LastLocationArg){LastLocation(); return CChipVal(CChipVal::TGT_DISTANCE);}
static CChipVal _targetDistanceXy	(LastLocationArg){LastLocation(); return CChipVal(CChipVal::TGT_DISTANCE_XY);}

static CChipVal _timeRemained		(LastLocationArg){LastLocation(); return CChipVal(CChipVal::TIME_REMAINED);}
static CChipVal _myId				(LastLocationArg){LastLocation(); return CChipVal(CChipVal::MY_ID);}
static CChipVal _myActCode			(LastLocationArg){LastLocation(); return CChipVal(CChipVal::MY_ACTCODE);}
static CChipVal _heat				(LastLocationArg){LastLocation(); return CChipVal(CChipVal::MY_HEAT);}
static CChipVal _health				(LastLocationArg){LastLocation(); return CChipVal(CChipVal::MY_HP);}
static CChipVal _energy				(LastLocationArg){LastLocation(); return CChipVal(CChipVal::MY_ENERGY);}
static CChipVal _mySpeed			(LastLocationArg){LastLocation(); return CChipVal(CChipVal::MY_SPEED);}
static CChipVal _targetSpeed		(LastLocationArg){LastLocation(); return CChipVal(CChipVal::TGT_SPEED);}
static CChipVal numAmmo(int weapon,  LastLocationArg){LastLocation(); return CChipVal(weapon == 0 ? CChipVal::NUM_AMMO0 : CChipVal::NUM_AMMO1 + weapon - 1);}
static CChipVal numOption(int option,LastLocationArg){LastLocation(); return CChipVal(CChipVal::NUM_OPTION1 + option - 1);}
static CChipVal _raderRange			(LastLocationArg){LastLocation(); return CChipVal(CChipVal::RADAR_RNAGE);}
static CChipVal _mathPi				(LastLocationArg){LastLocation(); return CChipVal(CChipVal::PI);}
static CChipVal _buttonValue		(LastLocationArg){LastLocation(); return CChipVal(CChipVal::BUTTON);}
static CChipVal _analogX			(LastLocationArg){LastLocation(); return CChipVal(CChipVal::ANALOG_X);}
static CChipVal _analogY			(LastLocationArg){LastLocation(); return CChipVal(CChipVal::ANALOG_Y);}
static CChipVal _gunsightDirection	(LastLocationArg){LastLocation(); return CChipVal(CChipVal::GUNSIGHT_DIRECTION);}
static CChipVal _gunsightElevation	(LastLocationArg){LastLocation(); return CChipVal(CChipVal::GUNSIGHT_ELEVATION);}
static CChipVal _gunsightNum		(LastLocationArg){LastLocation(); return CChipVal(CChipVal::GUNSIGHT_NO);}

static CChipVal mathInt	(double val, LastLocationArg) { LastLocation(); return CChipVal(CChipVal::MATH, std::make_unique<CChipCalc>(0, CChipCalc::INT, val)); }
static CChipVal mathAbs	(double val, LastLocationArg) { LastLocation(); return CChipVal(CChipVal::MATH, std::make_unique<CChipCalc>(0, CChipCalc::ABS, val)); }
static CChipVal mathMax	(double val, LastLocationArg) { LastLocation(); return CChipVal(CChipVal::MATH, std::make_unique<CChipCalc>(0, CChipCalc::MAX, val)); }
static CChipVal mathMin	(double val, LastLocationArg) { LastLocation(); return CChipVal(CChipVal::MATH, std::make_unique<CChipCalc>(0, CChipCalc::MIN, val)); }
static CChipVal mathSqr	(double val, LastLocationArg) { LastLocation(); return CChipVal(CChipVal::MATH, std::make_unique<CChipCalc>(0, CChipCalc::SQR, val)); }
static CChipVal mathSin	(double val, LastLocationArg) { LastLocation(); return CChipVal(CChipVal::MATH, std::make_unique<CChipCalc>(0, CChipCalc::SIN, val)); }
static CChipVal mathCos	(double val, LastLocationArg) { LastLocation(); return CChipVal(CChipVal::MATH, std::make_unique<CChipCalc>(0, CChipCalc::COS, val)); }
static CChipVal mathTan	(double val, LastLocationArg) { LastLocation(); return CChipVal(CChipVal::MATH, std::make_unique<CChipCalc>(0, CChipCalc::TAN, val)); }
static CChipVal mathAtan(double val, LastLocationArg) { LastLocation(); return CChipVal(CChipVal::MATH, std::make_unique<CChipCalc>(0, CChipCalc::ATAN, val)); }
static CChipVal mathNot	(double val, LastLocationArg) { LastLocation(); return CChipVal(CChipVal::MATH, std::make_unique<CChipCalc>(0, CChipCalc::NOT, val)); }

static CChipVal mathInt	(CChipVar& var, LastLocationArg) { LastLocation(); return CChipVal(CChipVal::MATH, std::make_unique<CChipCalc>(0, CChipCalc::INT, var.m_var)); }
static CChipVal mathAbs	(CChipVar& var, LastLocationArg) { LastLocation(); return CChipVal(CChipVal::MATH, std::make_unique<CChipCalc>(0, CChipCalc::ABS, var.m_var)); }
static CChipVal mathMax	(CChipVar& var, LastLocationArg) { LastLocation(); return CChipVal(CChipVal::MATH, std::make_unique<CChipCalc>(0, CChipCalc::MAX, var.m_var)); }
static CChipVal mathMin	(CChipVar& var, LastLocationArg) { LastLocation(); return CChipVal(CChipVal::MATH, std::make_unique<CChipCalc>(0, CChipCalc::MIN, var.m_var)); }
static CChipVal mathSqr	(CChipVar& var, LastLocationArg) { LastLocation(); return CChipVal(CChipVal::MATH, std::make_unique<CChipCalc>(0, CChipCalc::SQR, var.m_var)); }
static CChipVal mathSin	(CChipVar& var, LastLocationArg) { LastLocation(); return CChipVal(CChipVal::MATH, std::make_unique<CChipCalc>(0, CChipCalc::SIN, var.m_var)); }
static CChipVal mathCos	(CChipVar& var, LastLocationArg) { LastLocation(); return CChipVal(CChipVal::MATH, std::make_unique<CChipCalc>(0, CChipCalc::COS, var.m_var)); }
static CChipVal mathTan	(CChipVar& var, LastLocationArg) { LastLocation(); return CChipVal(CChipVal::MATH, std::make_unique<CChipCalc>(0, CChipCalc::TAN, var.m_var)); }
static CChipVal mathAtan(CChipVar& var, LastLocationArg) { LastLocation(); return CChipVal(CChipVal::MATH, std::make_unique<CChipCalc>(0, CChipCalc::ATAN,var.m_var)); }
static CChipVal mathNot	(CChipVar& var, LastLocationArg) { LastLocation(); return CChipVal(CChipVal::MATH, std::make_unique<CChipCalc>(0, CChipCalc::NOT, var.m_var)); }

static CChipVal chReceive(int ch, LastLocationArg){LastLocation(); return CChipVal(CChipVal::CH_RECV, ch);}

#ifndef NO_OKECC_SYNTAX
CChipVar& CChipVar::operator=(const CChipVal& val){

	std::unique_ptr<CChip> pchip;

	switch(val.m_type){
		case CChipVal::CH_RECV:
			pchip = std::make_unique<CChipChReceive>(val.m_param, m_var);
			break;

		case CChipVal::MATH: {
			std::unique_ptr<CChipCalc> pchip_calc = std::make_unique<CChipCalc>(0U, CChipCalc::MOV, 0U);
			*pchip_calc.get() = *static_cast<CChipCalc*>(val.m_chip.get());
			pchip_calc->m_op1 = m_var;
			pchip = std::move(pchip_calc);
			break;
		}

		default:
			if(g_bChp && val.m_type >= CChipVal::TIME_REMAINED){
				Error("Invalid use of = operator");
			}else{
				pchip = std::make_unique<CChipGetStatus>(val.m_type, m_var);
			}
	}

	g_pCurField->m_tree.AddToTree(std::move(pchip));
	return *this;
}

auto& CChipVal::GetCChipCond(void) const {
	
	std::unique_ptr<CChipCond> pchip;
	
	switch(m_type){
		case CChipVal::TIME:
			pchip = std::make_unique<CChipIfTime>(CChipIfTime::START);
			break;
			
		case CChipVal::TIME_REMAINED:
			pchip = std::make_unique<CChipIfTime>(CChipIfTime::END);
			break;
		
		case CChipVal::TGT_BODYCODE:
			pchip = std::make_unique<CChipIfBodyCode>();
			break;
		
		default:
			if(CChipVal::MY_HEAT <= m_type && m_type <= CChipVal::MY_ENERGY){
				pchip = std::make_unique<CChipSelfStatus>(
					m_type == CChipVal::MY_HEAT	? CChipSelfStatus::HEAT :
					m_type == CChipVal::MY_HP	? CChipSelfStatus::HP :
												  CChipSelfStatus::ENERGY
				);
			}
			
			else if(CChipVal::NUM_AMMO1 <= m_type && m_type <= CChipVal::NUM_OPTION4){
				pchip = std::make_unique<CChipAmmoNum>(m_type - CChipVal::NUM_AMMO1 + 1);
			}
			
		#ifndef CHP
			else if(CChipVal::NUM_AMMO0 == m_type){
				pchip = std::make_unique<CChipAmmoNum>(0);
			}
			
			else if(CChipVal::ANALOG_X == m_type || CChipVal::ANALOG_Y == m_type){
				pchip = std::make_unique<CChipIsAnalog>(m_type == ANALOG_X ? CChipIsAnalog::X : CChipIsAnalog::Y);
			}
		#endif
	}
	
	if(!pchip){
		Error("Invalid use of compare operator");
	}
	
	return *g_pCurField->m_tree.AddToPool(std::move(pchip));
}

CChipTree CChipVal::operator<=(int num) const {return GetCChipCond() <= num;}
CChipTree CChipVal::operator< (int num) const {return GetCChipCond() <  num;}
CChipTree CChipVal::operator>=(int num) const {return GetCChipCond() >= num;}
CChipTree CChipVal::operator> (int num) const {return GetCChipCond() >  num;}
CChipTree CChipVal::operator==(int num) const {return GetCChipCond() == num;}
CChipTree CChipVal::operator!=(int num) const {return GetCChipCond() != num;}
#endif

//////////////////////////////////////////////////////////////////////////////
// 算術比較

class CChipCmp : public CChipCond {
public:
	CChipCmp(
		UINT op1,
		UINT opr,
		UINT op2
	){
		m_op1		= op1;
		m_operator	= opr;
		m_immxvar	= 0;
		m_op2		= op2;
		m_imm		= 0;
		m_Id		= CHIPID_IF_CMP;
	}

	CChipCmp(
		UINT op1,
		UINT opr,
		double op2
	){
		m_op1		= op1;
		m_operator	= opr;
		m_immxvar	= 1;
		m_imm		= double2i32(op2);
		m_op2		= 0;
		m_Id		= CHIPID_IF_CMP;
	}

	virtual ~CChipCmp(){}

	virtual std::string GetLayoutText(void){
		return m_immxvar.get() ?
			std::format("{}{}\n{}?", m_VarNameStr[m_op1.get()], m_operator_str[m_operator.get()], int2fixp32s(m_imm.get())) :
			std::format("{}{}{}?", m_VarNameStr[m_op1.get()], m_operator_str[m_operator.get()], m_VarNameStr[m_op2.get()]);
	}

	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_imm.GetBin(bin);
		m_op1.GetBin(bin);
		m_op2.GetBin(bin);
		m_immxvar.GetBin(bin);
		m_operator.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		m_imm.SetBin(bin);
		m_op1.SetBin(bin);
		m_op2.SetBin(bin);
		m_immxvar.GetBin(bin);
		m_operator.SetBin(bin);
	}

	ScaledInt<32>	m_imm;
	ScaledInt<4>	m_op1;
	ScaledInt<3>	m_op2;
	ScaledInt<1>	m_immxvar;
	ScaledInt<>		m_operator;
};

#ifndef NO_OKECC_SYNTAX
CChipTree CChipVar::operator>=(const CChipVar& op2) const {return *g_pCurField->m_tree.AddToPool(std::make_unique<CChipCmp>(m_var, CChip::OP_GE, op2.m_var));}
CChipTree CChipVar::operator<=(const CChipVar& op2) const {return *g_pCurField->m_tree.AddToPool(std::make_unique<CChipCmp>(m_var, CChip::OP_LE, op2.m_var));}
CChipTree CChipVar::operator==(const CChipVar& op2) const {return *g_pCurField->m_tree.AddToPool(std::make_unique<CChipCmp>(m_var, CChip::OP_EQ, op2.m_var));}
CChipTree CChipVar::operator!=(const CChipVar& op2) const {return !(*this == op2);}
CChipTree CChipVar::operator> (const CChipVar& op2) const {return !(*this <= op2);}
CChipTree CChipVar::operator< (const CChipVar& op2) const {return !(*this >= op2);}

CChipTree CChipVar::operator>=(const double imm) const {return *g_pCurField->m_tree.AddToPool(std::make_unique<CChipCmp>(m_var, CChip::OP_GE, imm));}
CChipTree CChipVar::operator<=(const double imm) const {return *g_pCurField->m_tree.AddToPool(std::make_unique<CChipCmp>(m_var, CChip::OP_LE, imm));}
CChipTree CChipVar::operator==(const double imm) const {return *g_pCurField->m_tree.AddToPool(std::make_unique<CChipCmp>(m_var, CChip::OP_EQ, imm));}
CChipTree CChipVar::operator!=(const double imm) const {return !(*this == imm);}
CChipTree CChipVar::operator> (const double imm) const {return !(*this <= imm);}
CChipTree CChipVar::operator< (const double imm) const {return !(*this >= imm);}
#endif

//////////////////////////////////////////////////////////////////////////////
// ガンサイト移動

#ifndef CHP
class CChipAimGunsight : public CChip {
public:
	enum {
		ABS,
		REL,
	};
	
	CChipAimGunsight(
		double dir,
		double elev
	){
		m_Id			= CHIPID_AIM_GUNSIGHT;
		
		if(dir < 0) dir += 360;
		m_dir			= (int16_t)double2i32(dir);
		m_elev			= (int16_t)double2i32(elev);
	}

	CChipAimGunsight(
		CChipVar& dir,
		CChipVar& elev
	){
		m_Id			= CHIPID_AIM_GUNSIGHT_COUNTER;
		m_var_dir		= dir.m_var;
		m_var_elev		= elev.m_var;
	}

	virtual ~CChipAimGunsight(){}

	virtual std::string GetLayoutText(void){
		std::string s = std::format("GS{}移動\n方位=", m_mode.get() == REL ? "相対" : "");
		
		if(m_Id.get() == CHIPID_AIM_GUNSIGHT){
			int dir = (int16_t)m_dir.get();
			if(dir > 18000) dir -= 36000;
			
			return s + std::format("{}\n仰角={}",
				int2fixp32s(dir),
				int2fixp32s((int16_t)m_elev.get())
			);
		}
		
		return std::format("{}\n仰角={}",
			m_VarNameStr[m_var_dir.get()],
			m_VarNameStr[m_var_elev.get()]
		);
	}

	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		
		if(m_Id.get() == CHIPID_AIM_GUNSIGHT){
			m_mode.GetBin(bin);
			bin.m_pos += 8;
			m_dir.GetBin(bin);
			m_elev.GetBin(bin);
		}else{
			m_var_dir.GetBin(bin);
			m_var_elev.GetBin(bin);
			m_mode.GetBin(bin);
		}
	}

	// option
	auto& _rel()	{m_mode = REL;	return *this;}

	ScaledInt<>						m_mode;
	ScaledInt<16, -9000, 9000>		m_elev;
	ScaledInt<16, 0, 36000>			m_dir;
	ScaledInt<>						m_var_elev;
	ScaledInt<>						m_var_dir;

	int m_target = 0;
};

static auto& aimGunsight(
	double dir,
	double elev,
	LastLocationArg
){
	LastLocation();
	return *g_pCurField->m_tree.AddToTree(std::make_unique<CChipAimGunsight>(dir, elev));
}

static auto& aimGunsight(
	CChipVar& dir,
	CChipVar& elev,
	LastLocationArg
){
	LastLocation();
	return *g_pCurField->m_tree.AddToTree(std::make_unique<CChipAimGunsight>(dir, elev));
}

//////////////////////////////////////////////////////////////////////////////
// ガンサイト起動

class CChipGunsight : public CChip {
public:
	CChipGunsight(UINT param){
		m_Id	= CHIPID_GUNSIGHT_ON;
		m_param	= param - 1;
	}

	virtual ~CChipGunsight(){}

	virtual std::string GetLayoutText(void){
		std::string s = "ガンサイト\n";
		
		if(m_param.get() < 5){
			s += std::format("起動 武装{}", m_param.get() + 1);
		}else if(m_param.get() == 5){
			s += "OFF";
		}else{
			s += "起動 次武装";
		}
		return s;
	}

	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_param.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		m_param.SetBin(bin);
	}

	ScaledInt<8, 0, 6> m_param;
};

static void gunsight(UINT param, LastLocationArg){
	LastLocation();
	g_pCurField->m_tree.AddToTree(std::make_unique<CChipGunsight>(param));
}

//////////////////////////////////////////////////////////////////////////////
// カメラビュー

class CChipView : public CChip {
public:
	enum {
		BACK,
		FRONT,
		LEFT,
		RIGHT,
		TOP,
		GUNSIGHT,
		PAIR,
		NEXT,
	};
	
	static inline const char* m_ViewStr[] = {
		"後方", "前方", "左", "右", "トップ", "GS", "ペア", "次の"
	};
	
	enum {
		VIEW_FAR,
		VIEW_MIDDLE,
		VIEW_NEAR,
	};
	
	static inline const char* m_DistStr[] = {
		"遠", "中", "近"
	};
	
	CChipView(UINT param){
		m_Id	= CHIPID_VIEW;
		m_param	= param;
		m_dist	= VIEW_MIDDLE;
	}

	virtual ~CChipView(){}

	virtual std::string GetLayoutText(void){
		return std::format(
			"{}ビュー\n{}距離",
			m_ViewStr[m_param.get()],
			m_DistStr[m_dist.get()]
		);
	}

	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_param.GetBin(bin);
		m_dist.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		m_param.SetBin(bin);
		m_dist.SetBin(bin);
	}

	// option
	auto& _near()		{m_dist	= VIEW_NEAR;	return *this;}
	auto& _far()		{m_dist	= VIEW_FAR;		return *this;}
	
	ScaledInt<> m_param;
	ScaledInt<> m_dist;
};

static auto& _viewBack		(LastLocationArg){LastLocation(); return *g_pCurField->m_tree.AddToTree(std::make_unique<CChipView>(CChipView::BACK));}
static auto& _viewFront		(LastLocationArg){LastLocation(); return *g_pCurField->m_tree.AddToTree(std::make_unique<CChipView>(CChipView::FRONT));}
static auto& _viewLeft		(LastLocationArg){LastLocation(); return *g_pCurField->m_tree.AddToTree(std::make_unique<CChipView>(CChipView::LEFT));}
static auto& _viewRight		(LastLocationArg){LastLocation(); return *g_pCurField->m_tree.AddToTree(std::make_unique<CChipView>(CChipView::RIGHT));}
static auto& _viewTop		(LastLocationArg){LastLocation(); return *g_pCurField->m_tree.AddToTree(std::make_unique<CChipView>(CChipView::TOP));}
static auto& _viewGunsight	(LastLocationArg){LastLocation(); return *g_pCurField->m_tree.AddToTree(std::make_unique<CChipView>(CChipView::GUNSIGHT));}
static auto& _viewPair		(LastLocationArg){LastLocation(); return *g_pCurField->m_tree.AddToTree(std::make_unique<CChipView>(CChipView::PAIR));}
static auto& _viewNext		(LastLocationArg){LastLocation(); return *g_pCurField->m_tree.AddToTree(std::make_unique<CChipView>(CChipView::NEXT));}

//////////////////////////////////////////////////////////////////////////////
// Alert

class CChipAlert : public CChip {
public:
	
	CChipAlert(
		int sound		= 1,
		int time		= 3,
		int place		= AL_NONE
	){
		m_Id		= CHIPID_ALERT;
		
		if(--sound < 0) sound = 5;
		m_sound		= sound;
		m_place		= place;
		m_time		= time;
	}

	virtual ~CChipAlert(){}

	static inline const char *m_PlaceStr[] = {
		"右", "左", "上", "下", "中央", "無"
	};
	
	virtual std::string GetLayoutText(void){
		std::string s = "アラート音";
		
		if(m_sound.get() >= 5){
			s += "無";
		}else{
			s += std::format("{}", m_sound.get() + 1);
		}
		
		s += std::format("\n表示{}\n時間 {}s", m_PlaceStr[m_place.get()], m_time.get());
		
		return s;
	}

	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_sound.GetBin(bin);
		m_place.GetBin(bin);
		m_time.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		m_sound.SetBin(bin);
		m_place.SetBin(bin);
		m_time.SetBin(bin);
	}

	ScaledInt<>	m_sound;
	ScaledInt<>	m_place;
	ScaledInt<>	m_time;
};

static auto& alert(
	int sound		= 1,
	int time		= 3,
	int place		= AL_NONE,
	LastLocationArg
){
	LastLocation();
	return *g_pCurField->m_tree.AddToTree(std::make_unique<CChipAlert>(sound, time, place));
}

#endif

//////////////////////////////////////////////////////////////////////////////
// if - else - endif

static void if_statement(const CChipTree& cc, LastLocationArg, bool BlockStart = true){
	LastLocation();

	if(BlockStart){
		g_pCurField->m_BlockStack.push_back(std::make_unique<CField::BiIfTop>(location));
	}

	// CurTree に if 条件のツリーを接続
	g_pCurField->m_tree.AddToG(cc.m_start);
	g_pCurField->m_tree.m_LastG = cc.m_LastR;

	// false 飛び先
	g_pCurField->m_BlockStack.push_back(std::make_unique<CField::BiIf>(location, cc.m_LastG));
}

static void if_statement(int cc, LastLocationArg, bool BlockStart = true){
	LastLocation();

	if(cc != 0){
		Error("Constant condition always evaluates to false");
	}

	if(BlockStart){
		g_pCurField->m_BlockStack.push_back(std::make_unique<CField::BiIfTop>(location));
	}

	// false 用 goto を生成し CurTree に接続
	UINT false_chip;
	g_pCurField->m_tree.AddToG(false_chip = g_pCurField->m_pool.add(std::make_unique<CChipGoto>()));

	// true 用 goto 生成 (ただし実行されない dead code になる)
	g_pCurField->m_tree.m_LastG = g_pCurField->m_pool.add(std::make_unique<CChipGoto>());

	// false 飛び先
	g_pCurField->m_BlockStack.push_back(std::make_unique<CField::BiIf>(location, false_chip));
}

static void if_statement(const CChipVal& chip, LastLocationArg){if_statement(chip >= 1, location);}
static void if_statement(const CChipVar& chip, LastLocationArg){if_statement(chip != 0, location);}

static void else_statement(
	LastLocationArg
){
	LastLocation();

	if(g_pCurField->GetMode() != CField::BM_IF){
		g_pCurField->BlockError("Unexpected else / elseif");
		if(!g_pCurField->m_BlockStack.size()) return;
	}

	auto bi = std::move(g_pCurField->m_BlockStack.back());
	g_pCurField->m_BlockStack.pop_back();

	UINT idx = g_pCurField->m_tree.m_LastG;	// then 節の最後

	// else 節先頭は，BlockStack に積んでいた false 飛び先
	g_pCurField->m_tree.m_LastG = bi->m_TargetIdx;

	// then 節の最後を block statck に積む
	g_pCurField->m_BlockStack.push_back(std::make_unique<CField::BiElse>(location, idx));
}

static void elseif_statement(const CChipTree& cc, LastLocationArg){
	LastLocation();
	else_statement(location);
	if_statement(cc, location, false);
}

static void elseif_statement(const CChipVal& chip, LastLocationArg){elseif_statement(chip >= 1, location);}
static void elseif_statement(const CChipVar& chip, LastLocationArg){elseif_statement(chip != 0, location);}

static void endif_statement(
	LastLocationArg
){
	LastLocation();

	while(
		g_pCurField->GetMode() == CField::BM_IF ||
		g_pCurField->GetMode() == CField::BM_ELSE
	){
		auto bi = std::move(g_pCurField->m_BlockStack.back());
		g_pCurField->m_BlockStack.pop_back();

		// 合流 GOTO 生成
		UINT merge = g_pCurField->m_pool.add(std::make_unique<CChipGoto>());
		g_pCurField->m_tree.AddToG(merge);

		// false 条件の飛び先合流
		g_pCurField->m_pool[bi->m_TargetIdx]->m_NextG = merge;
	}

	if(g_pCurField->GetMode() != CField::BM_IF_TOP){
		g_pCurField->BlockError("Internal Error: Block stack broken");
		if(!g_pCurField->m_BlockStack.size()) return;
	}

	g_pCurField->m_BlockStack.pop_back();
}

//////////////////////////////////////////////////////////////////////////////
// Loop statement

static void loop_statement(LastLocationArg){
	LastLocation();

	UINT LoopTop = 0;

	// goto を 2個生成
	g_pCurField->m_BlockStack.push_back(
		std::make_unique<CField::BiLoop>(
			location,
			LoopTop = g_pCurField->m_pool.add(std::make_unique<CChipGoto>()),	// loop 先頭
			g_pCurField->m_Break								// 直前の break 先を保存
		)
	);

	g_pCurField->m_Break = g_pCurField->m_pool.add(std::make_unique<CChipGoto>());	// break 先

	// Top の goto を接続
	g_pCurField->m_tree.AddToG(LoopTop);
}

static void endloop_statement(LastLocationArg){
	LastLocation();

	auto bi_ptr = std::move(g_pCurField->m_BlockStack.back());
	g_pCurField->m_BlockStack.pop_back();

	auto bi = dynamic_cast<CField::BiLoop*>(bi_ptr.get()); // mode

	// ループ先頭に接続
	g_pCurField->m_tree.AddToG(bi->m_TargetIdx);
	g_pCurField->m_tree.m_LastG = g_pCurField->m_Break;

	// break 先を pop
	g_pCurField->m_Break = bi->m_SavePrevBreak;
}

static void break_statement(LastLocationArg){
	LastLocation();

	if(g_pCurField->m_Break == IDX_NONE){
		g_pCurField->BlockError("break not within a loop");
	}

	UINT idx = g_pCurField->m_tree.m_LastG;

	// LastG の place holder
	g_pCurField->m_tree.AddToG(g_pCurField->m_pool.add(std::make_unique<CChipGoto>()));

	// break 先に分岐
	g_pCurField->m_pool[idx]->m_NextG = g_pCurField->m_Break;
}

//////////////////////////////////////////////////////////////////////////////
// end: endif と endwhile 共通

static void end_statement(LastLocationArg){
	LastLocation();
	
	if(
		g_pCurField->GetMode() == CField::BM_IF ||
		g_pCurField->GetMode() == CField::BM_ELSE
	){
		endif_statement(location);
	}

	else if(g_pCurField->GetMode() == CField::BM_LOOP){
		endloop_statement(location);
	}
	
	else{
		g_pCurField->BlockError("Unexpected End");
	}
}

//////////////////////////////////////////////////////////////////////////////
// return

static void okeccReturn(LastLocationArg){
	LastLocation();

	// Exit に飛ばす chip の idx
	auto idx = g_pCurField->m_tree.m_LastG;

	// 後続 chip を接続する用の goto 生成
	g_pCurField->m_tree.AddToTree(std::make_unique<CChipGoto>());

	// 実際に Exit (or ISub return) に向ける
	g_pCurField->m_pool[idx]->m_NextG = g_pCurField->m_uISubRetIdx;
}

//////////////////////////////////////////////////////////////////////////////
// start sub

class CFieldSwitch {
public:
	CField	*m_field;

	CFieldSwitch(int num){
		m_field	= g_pCurField;
		g_pCurField	= g_pField[num].get();
	}

	~CFieldSwitch(){
		// block stack チェック
		g_pCurField->CheckBlockStack();

		g_pCurField	= m_field;
	}
};

inline bool g_PutSub[2] = {false, false};

static bool start_sub_internal(int num, LastLocationArg){
	LastLocation();

	if(num < 1 || num > 2){
		Error("Subroutine num. must be between 1 and 2.");
		return false;
	}

	put_sub_chip(num, location);

	if(g_PutSub[num - 1]) return false; // すでに sub 構築済み

	g_PutSub[num - 1] = true;
	return true;
}

//////////////////////////////////////////////////////////////////////////////
// Inline sub 開始

class CInlineSub {
public:
	UINT m_uPrevISubRetIdx = IDX_EXIT;

	CInlineSub(){
		// 直前の return 先を保存
		m_uPrevISubRetIdx = g_pCurField->m_uISubRetIdx;

		// return 用 goto を生成
		g_pCurField->m_uISubRetIdx = g_pCurField->m_pool.add(std::make_unique<CChipGoto>());
	}

	~CInlineSub(){
		// LastG を return 先に接続
		g_pCurField->m_tree.AddToG(g_pCurField->m_uISubRetIdx);

		// 直前の return 先を復元
		g_pCurField->m_uISubRetIdx = m_uPrevISubRetIdx;
	}
};

//////////////////////////////////////////////////////////////////////////////
// C との命名被り回避

#ifndef NO_OKECC_SYNTAX
	#define If(cc)		if_statement(cc);
	#define ElseIf(cc)	elseif_statement(cc);
	#define	Elseif		ElseIf
	#define Else		else_statement();
	#define End			end_statement();
	
	#define Break		break_statement()
	#define While(cc)	loop_statement(); If(!(cc)) Break; End
	#define Return		okeccReturn()

	#define startSub(num)	if(!start_sub_internal(num)) return; CFieldSwitch FieldInfo(num)
	#define startInlineSub	CInlineSub _virtual_sub_info

	#define autoTurn				_autoTurn()
	#define energy					_energy()
	#define fight					_fight()
	#define fightHigh				_fightHigh()
	#define fightLong				_fightLong()
	#define fightLow				_fightLow()
	#define health					_health()
	#define heat					_heat()
	#define isLineBlocked			_isLineBlocked()
	#define isOutsideArea			_isOutsideArea()
	#define isPositionFromTarget	_isPositionFromTarget()
	#define isSelfFighting			_isSelfFighting()
	#define isSelfFiring			_isSelfFiring()
	#define isSelfJumping			_isSelfJumping()
	#define isSelfMoving			_isSelfMoving()
	#define isSelfSpecial			_isSelfSpecial()
	#define isSelfStumbling			_isSelfStumbling()
	#define isSelfTurning			_isSelfTurning()
	#define isSelfWaiting			_isSelfWaiting()
	#define isTargetFighting		_isTargetFighting()
	#define isTargetFiring			_isTargetFiring()
	#define isTargetJumping			_isTargetJumping()
	#define isTargetMoving			_isTargetMoving()
	#define isTargetPosition		_isTargetPosition()
	#define isTargetSpecial			_isTargetSpecial()
	#define isTargetStumbling		_isTargetStumbling()
	#define isTargetTurning			_isTargetTurning()
	#define isTargetStopped			_isTargetWaiting()
#ifdef CHP
	#define isUnlock				_isUnlock()
#else
	#define isLocked				_isUnlock()
#endif
	#define jumpBackward			_jumpBackward()
	#define jumpForward				_jumpForward()
	#define jumpLeft				_jumpLeft()
	#define jumpRight				_jumpRight()
	#define jumpUp					_jumpUp()
	#define lockon					_lockon()
	#define lockonAll				_lockonAll()
	#define lockonFriendly			_lockonFriendly()
	#define mathRand				_mathRand()
	#define moveBackward			_moveBackward()
	#define moveForward				_moveForward()
	#define moveLeft				_moveLeft()
	#define moveRight				_moveRight()
	#define myDirection				_myDirection()
	#define myX						_myX()
	#define myY						_myY()
	#define myZ						_myZ()
	#define nop						_nop()
	#define numAllEnemy				_numAllEnemy()
	#define numAllFriendly			_numAllFriendly()
	#define numEnemy				_numEnemy()
	#define numFriendly				_numFriendly()
	#define numLocked				_numLocked()
	#define numAllOke				_numAllOke()
	#define numProjectile			_numProjectile()
	#define stop					_stop()
	#define targetActCode			_targetActCode()
	#define targetAzimuth			_targetAzimuth()
	#define targetBodyCode			_targetBodyCode()
	#define targetDirection			_targetDirection()
	#define targetDistance			_targetDistance()
	#define targetDistanceXy		_targetDistanceXy()
	#define targetElevation			_targetElevation()
	#define targetId				_targetId()
	#define targetX					_targetX()
	#define targetY					_targetY()
	#define targetZ					_targetZ()
	#define time					_time()
	#define timeRemained			_timeRemained()
	#define turnLeft				_turnLeft()
	#define turnRight				_turnRight()
	#define wait					_wait()
	#define myId					_myId()
	#define myActCode				_myActCode()
	#define heat					_heat()
	#define health					_health()
	#define energy					_energy()
	
#ifndef CHP
	#define numUnknownOke			_numUnknownOke()
	#define mySpeed					_mySpeed()
	#define targetSpeed				_targetSpeed()
	#define raderRange				_raderRange()
	#define mathPi					_mathPi()
	#define buttonValue				_buttonValue()
	#define analogX					_analogX()
	#define analogY					_analogY()
	#define gunsightDirection		_gunsightDirection()
	#define gunsightElevation		_gunsightElevation()
	#define gunsightNum				_gunsightNum()
	#define getButton				_getButton()
	#define isGunsightActive		_isGunsightActive()
	#define isAutoTurnActive		_isAutoTurnActive()
	#define isTargetSheldActive		_isTargetSheldActive()
	#define isTargetOverheat		_isTargetOverheat()
	#define isMutualLock			_isMutualLock()
	#define isLineBlockedByTerrain	_isLineBlockedTerrain()
	#define isLineBlockedByFriendly	_isLineBlockedFriendly()
	#define isLineBlockedByBarrier	_isLineBlockedBarrier()
	#define waitMove				_waitMove()
	#define waitTurn				_waitTurn()
	#define waitFire				_waitFire()
	#define stopMove				_stopMove()
	#define stopTurn				_stopTurn()
	#define stopFire				_stopFire()
	#define moveForwardLeft			_moveForwardLeft()
	#define moveForwardRight		_moveForwardRight()
	#define moveBackwardLeft		_moveBackwardLeft()
	#define moveBackwardRight		_moveBackwardRight()
	#define jumpForwardLeft			_jumpForwardLeft()
	#define jumpForwardRight		_jumpForwardRight()
	#define jumpBackwardLeft		_jumpBackwardLeft()
	#define jumpBackwardRight		_jumpBackwardRight()
	#define ascend					_ascend()
	#define descend					_descend()
	#define unlock					_unlock()
	#define viewBack				_viewBack()
	#define viewFront				_viewFront()
	#define viewLeft				_viewLeft()
	#define viewRight				_viewRight()
	#define viewTop					_viewTop()
	#define viewGunsight			_viewGunsight()
	#define viewPair				_viewPair()
	#define viewNext				_viewNext()
	
	#define farthest				_farthest()
	#define front					_front()
	#define rel						_rel()
	#define near					_near()
	#define far						_far()
#endif

	#define	fast					_fast()
	#define wide					_wide()
	#define snipe					_snipe()
	#define target					_target()
	#define off						_off()
#endif
