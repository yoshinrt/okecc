#include <algorithm>
#include <concepts>
#include <format>
#include <source_location>
#include <stdexcept>
#include <vector>
#include <memory>
#include <functional>
#include <cmath>

enum {
	OKE_BIPED,
	OKE_QUADRUPED,
	OKE_HOVER,
	OKE_VEHICLE,
	OKE_FLIGHT,
	OKE_ALL,
};

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
	W_ASSULT,
	W_BEAM,
	W_PULSE,
	W_NAPALM,
	W_FLANK,
	W_SHOOTGUN,
	W_CANNON,
	W_RAILGUN,
	W_GRANADE,
	W_BOMB,
	W_ROCKET,
	W_MISSILE,
	W_MINE,
	W_FMINE,
};

enum {
	BODY
};

typedef unsigned int UINT;

static constexpr UINT IDX_NONE = 0xFFFFFFFF;
static constexpr UINT IDX_EXIT = 0xFFFFFFFE;
constexpr int DEFAULT_INT = 0x7FFFFFFF;

//////////////////////////////////////////////////////////////////////////////

inline std::source_location g_LastLocation = std::source_location::current();
#define LastLocation()	(g_LastLocation = location)
#define LastLocationArg	const std::source_location location = std::source_location::current()

inline UINT g_uErrorCnt = 0;

static inline void Error(const std::string& message){
	puts(std::format("{}({}): Error: {}", g_LastLocation.file_name(), g_LastLocation.line(), message).c_str());
	++g_uErrorCnt;
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
		CHIPID_SUB					= 0xDA,
		CHIPID_GET_STATUS			= 0xEA,
		CHIPID_CALC					= 0xEB,
		CHIPID_CH_SEND				= 0xEF,
		CHIPID_CH_RECV				= 0xF0,
		CHIPID_IF_AMMO_NUM			= 0xD1,
		CHIPID_IF_OUTSIDE_AREA		= 0xD2,
		CHIPID_IF_BARRIER			= 0xD4,
		CHIPID_IF_OKE_NUM			= 0xD3,
		CHIPID_IF_DET_PROJECTILE	= 0xD5,
		CHIPID_IF_SELF_STATUS		= 0xD6,
		CHIPID_IF_MY_ACTION			= 0xD7,
		CHIPID_IF_TGT_ACTION		= 0xDE,
		CHIPID_IF_RAND				= 0xD8,
		CHIPID_IF_TGT_POS			= 0xDC,
		CHIPID_IF_POS_FROM_TGT		= 0xDD,
		CHIPID_IF_TIME				= 0xD9,
		CHIPID_IF_CMP				= 0xEE,
		CHIPID_STOP					= 0xC4,
		CHIPID_MOVE					= 0xC5,
		CHIPID_TURN					= 0xC6,
		CHIPID_JUMP					= 0xC7,
		CHIPID_FAST_MOVE			= 0xC8,
		CHIPID_FAST_TURN			= 0xC9,
		CHIPID_FIGHT				= 0xCA,
		CHIPID_GUARD				= 0xCB,
		CHIPID_SPECIAL				= 0xCC,
		CHIPID_FIRE_NEAREST			= 0xCE,
		CHIPID_FIRE_TGT				= 0xE2,
		CHIPID_FIRE_FIXED_DIR		= 0xCF,
		CHIPID_FIRE_COUNTER			= 0xED,
		CHIPID_JUMP_TURN			= 0xE3,
		CHIPID_MOVE_TURN			= 0xE4,
		CHIPID_FIRE_MOVE			= 0xE5,
		CHIPID_FIRE_JMP				= 0xE6,
		CHIPID_ALTITUDE				= 0xCD,
		CHIPID_OPTION				= 0xD0,
		CHIPID_TGT_BODYCODE			= 0xDF,
		CHIPID_NUM_LOCKED			= 0xE0,
		CHIPID_WEAPON_ID			= 0xE1,
		CHIPID_IS_LINE_CLEAR		= 0xE9,
		CHIPID_LOCKON				= 0xDB,
		CHIPID_LOCKON_COUNTER		= 0xEC,
		CHIPID_LOCKON_PARTS			= 0xE8,
		CHIPID_AUTO_TURN			= 0xE7,
		CHIPID_GOTO					= 0xFF,
	};
	
	static inline const char *m_OkeTypeStr[] = {
		"二脚",
		"四脚",
		"ホバー",
		"車両",
		"飛行",
		"全種",
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
		UP,
	};

	static inline const char* m_move_str[] = {
		"左", "右", "前", "後", "上"
	};

	static inline const char *m_StatusTypeStr[] = {
		"敵数", "味方数", "時間", "rand", "自機X", "自機Y", "自機Z", "自機向き", "TGT ID", "TGT方向", "TGT仰角", "TGT X", "TGT Y", "TGT Z", "TGT向き", "TGT機体#", "TGT動作#", "TGT距離", "TGT距離(XY)"
	};

	static inline const char *m_SelfStatusTypeStr[] = {
		"HP", "燃料", "熱量"
	};

	enum {
		ENEMY,
		FRIENDLY,
		ENEMY_FRIENDLY,
	};

	static inline const char *m_EnemyFriendlyStr[] = {
		"敵", "味方", "両軍",
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

	virtual void num_set(int num){}
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

	CChipTree(std::unique_ptr<CChipCond> upchip, CChipPool& pool) : m_pool(pool){
		// チップ単体からツリーに変換 (Condition chip)
		m_start = m_pool.add(std::move(upchip));
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
		m_pool[m_start]->num_set(num);
		m_pool[m_start]->set_operator(CChip::OP_GE);
		return *this;
	}

	CChipTree& operator<=(int num) {
		m_pool[m_start]->num_set(num);
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
		m_pool[m_start]->num_set(num);
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

	CField(const char *name, UINT width, UINT height) : m_name(name), m_pool(width, height), m_tree(m_pool){}

	int FinalizeCompile(){
		m_tree.AddToG(IDX_EXIT);
		m_pool.m_start = m_tree.m_start;
		//m_pool.dump();

		if(m_pool.size() > 0){
			// 参照されないチップを goto に置き換える
			m_pool.DeleteUnreferencedChips();

			// Goto 最適化
			m_pool.CleanupGoto();
		}
		printf("%s: Number of chip(s): %d\n", m_name, (int)m_pool.m_list.size());

		// field 最大チップ数を超えていたらエラー
		if(m_pool.m_list.size() > CField::m_pool.m_width * CField::m_pool.m_height){
			printf("Number of chip(s) exceeds the maximum limit: %d > %d\n",
				(int)m_pool.m_list.size(),
				CField::m_pool.m_width * CField::m_pool.m_height
			);

			return -1;
		}

		return (int)m_pool.m_list.size();
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

	CChipVar& operator+=(const double op2);
	CChipVar& operator-=(const double op2);
	CChipVar& operator*=(const double op2);
	CChipVar& operator/=(const double op2);
	CChipVar& operator%=(const double op2);
	CChipVar& operator= (const double op2);

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


static void _nop(
	LastLocationArg
){
	LastLocation();
	g_pCurField->m_tree.AddToTree(std::make_unique<CChipNop>());
}

class CChipSimple : public CChip {
	public:
	CChipSimple(UINT id, const std::string text) : m_text(text){
		m_Id = id;
	}
	virtual ~CChipSimple(){}
	virtual std::string GetLayoutText(void){
		return m_text;
	}

private:
	std::string m_text;
};

static void simple_chip(
	UINT id,
	const std::string& text,
	LastLocationArg
) {
	LastLocation();
	g_pCurField->m_tree.AddToTree(std::make_unique<CChipSimple>(id, text));
}

static void _wait(LastLocationArg){simple_chip(CChip::CHIPID_WAIT_AE, "Wait", location);}

class CChipWait : public CChip {
public:
	CChipWait(UINT param = 0){
		m_Id	= CHIPID_WAIT;
		m_param	= param;
	}

	virtual ~CChipWait(){}

	virtual std::string GetLayoutText(void){
		return std::format("Stop {:d}", m_param.get());
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

static void _stop(LastLocationArg){simple_chip(CChip::CHIPID_STOP, "Stop", location);}

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
	}

	// option
	auto& _fast()		{m_mode		= FAST;		return *this;}
	auto& _wait()		{m_exmode	= EM_WAIT;	return *this;}
	auto& _turnLeft()	{m_turn	= LEFT;  m_mode = TURN; return *this;}
	auto& _turnRight()	{m_turn	= RIGHT; m_mode = TURN; return *this;}

	ScaledInt<>	m_param;
	ScaledInt<>	m_turn;
	ScaledInt<>	m_exmode;

	int	m_mode = NORMAL;
};

static auto& put_move_chip(int param, LastLocationArg) {
	return *g_pCurField->m_tree.AddToTree(std::make_unique<CChipMove>(param));
}

static auto& _moveLeft		(LastLocationArg){LastLocation(); return put_move_chip(CChip::LEFT);}
static auto& _moveRight		(LastLocationArg){LastLocation(); return put_move_chip(CChip::RIGHT);}
static auto& _moveForward	(LastLocationArg){LastLocation(); return put_move_chip(CChip::FWD);}
static auto& _moveBackward	(LastLocationArg){LastLocation(); return put_move_chip(CChip::BACK);}

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
		return std::format("{}{}{}旋回", m_exmode_str[m_exmode.get()], m_fastmode_str[m_fast], m_move_str[m_param.get()]);
	}

	virtual void GetBin(CChipBinary& bin){
		if(m_fast) m_Id = CHIPID_FAST_TURN;
		CChip::GetBin(bin);
		m_param.GetBin(bin);
		if(m_fast) m_exmode.GetBin(bin);
	}

	// option
	auto& _fast()		{m_fast		= 1;		return *this;}
	auto& _wait()		{m_exmode	= EM_WAIT;	return *this;}

	ScaledInt<>	m_param;
	ScaledInt<>	m_exmode;

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
	auto& _turnLeft()	{m_turn	= LEFT;  m_with_turn = true; return *this;}
	auto& _turnRight()	{m_turn	= RIGHT; m_with_turn = true; return *this;}

	ScaledInt<> m_param;
	ScaledInt<>	m_turn;
	ScaledInt<> m_exmode;
	
	bool m_with_turn = false;
};

static auto& put_jump_chip(int param) {
	return *g_pCurField->m_tree.AddToTree(std::make_unique<CChipJump>(param));
}

static auto& _jumpForward	(LastLocationArg) { LastLocation(); return put_jump_chip(CChip::FWD);}
static auto& _jumpBackward	(LastLocationArg) { LastLocation(); return put_jump_chip(CChip::BACK);}
static auto& _jumpLeft		(LastLocationArg) { LastLocation(); return put_jump_chip(CChip::LEFT);}
static auto& _jumpRight		(LastLocationArg) { LastLocation(); return put_jump_chip(CChip::RIGHT);}

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
	CChipAlt(UINT param){
		m_Id	= CHIPID_ALTITUDE;
		m_param	= param;
	}

	virtual ~CChipAlt(){}

	virtual std::string GetLayoutText(void){
		return std::format("高度 {}m", m_param.get());
	}

	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_param.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		m_param.SetBin(bin);
	}

	ScaledInt<8, 20, 100> m_param;
};

static void setAltitude(int param, LastLocationArg){LastLocation();
g_pCurField->m_tree.AddToTree(std::make_unique<CChipAlt>(param));
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
		
		if(m_mode >= MOVE){
			m_movedir.GetBin(bin);
		}
		
		m_weapon.GetBin(bin);	if(m_mode != NORMAL) bin.m_pos += 5;
		m_cnt.GetBin(bin);		if(m_mode != NORMAL) bin.m_pos += 3;
		
		if(m_mode <= TGT){
			m_firemode.GetBin(bin);
			if(m_mode == TGT) bin.m_pos += 4;
		}
		m_exmode.GetBin(bin);
	}

	// option
	auto& _wait()			{m_exmode	= EM_WAIT;	return *this;}
	auto& _wide()			{m_firemode	= FM_WIDE;	return *this;}
	auto& _snipe()			{m_firemode	= FM_SNIPE;	return *this;}
	auto& _target()			{						m_mode	= TGT;	return *this;}
	auto& _moveLeft()		{m_movedir	= LEFT;		m_mode = MOVE; return *this;}
	auto& _moveRight()		{m_movedir	= RIGHT;	m_mode = MOVE; return *this;}
	auto& _moveForward()	{m_movedir	= FWD;		m_mode = MOVE; return *this;}
	auto& _moveBackward()	{m_movedir	= BACK; 	m_mode = MOVE; return *this;}
	auto& _jumpLeft()		{m_movedir	= LEFT; 	m_mode = JUMP; return *this;}
	auto& _jumpRight()		{m_movedir	= RIGHT;	m_mode = JUMP; return *this;}
	auto& _jumpForward()	{m_movedir	= FWD;		m_mode = JUMP; return *this;}
	auto& _jumpBackward()	{m_movedir	= BACK; 	m_mode = JUMP; return *this;}
	
	ScaledInt<3, 1, 5, 1, 1>	m_weapon;
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
	ScaledInt<8, 1, 5, 1, 1>	m_weapon;
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
		m_weapon	= weapon;
		m_operator	= opr;
		m_num		= num;
		m_Id		= CHIPID_IF_AMMO_NUM;
	}

	virtual ~CChipAmmoNum(){}

	virtual void num_set(int num){m_num = num;}
	virtual void set_operator(int opr){m_operator = opr;}

	virtual std::string GetLayoutText(void){
		return
			std::format(
				"{}#{}\n残数{}{}?",
				(m_weapon.get() < 5 ? "武装" : "オプション"), (m_weapon.get() % 5 + 1),
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

static CChipTree numAmmo(
	int weapon,
	LastLocationArg
){
	LastLocation();
	return CChipTree(std::make_unique<CChipAmmoNum>(weapon - 1), g_pCurField->m_pool);
}

static CChipTree numOption(
	int weapon,
	LastLocationArg
){
	LastLocation();
	return CChipTree(std::make_unique<CChipAmmoNum>(weapon + 4), g_pCurField->m_pool);
}

//////////////////////////////////////////////////////////////////////////////
// 近くの OKE を探索

class CChipOkeNum : public CChipCond {
public:
	#include "opt_coordinate.h"
	
	CChipOkeNum(
		int enemy
	){
		m_type		= OKE_ALL;
		m_enemy		= enemy;
		m_Id		= CHIPID_IF_OKE_NUM;
		m_num		= 1;
		InitCoordinate();
	}

	virtual ~CChipOkeNum(){}

	virtual void num_set(int num){m_num = num;}
	virtual void set_operator(int opr){m_operator = opr;}

	virtual std::string GetLayoutText(void){
		return
			std::format(
				"{}{}{}{}?\n{}",
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
	auto& type(int param)	{m_type = param;	return *this;}
	
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
static auto& _numOke		(LastLocationArg){LastLocation(); return numOke(CChip::ENEMY_FRIENDLY); }

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

	virtual void num_set(int num){m_barrier_height = num;}
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

	virtual void num_set(int num){m_num = num;}
	virtual void set_operator(int opr){m_operator = opr;}

	virtual std::string GetLayoutText(void){
		return
			std::format(
				"{}{}{}?\n{}",
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

	virtual void num_set(int num){m_num = num;}
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

static CChipSelfStatus& _health(
	LastLocationArg
){
	LastLocation();
	return *g_pCurField->m_tree.AddToPool(std::make_unique<CChipSelfStatus>(CChipSelfStatus::HP));
}

static CChipTree _energy(
	LastLocationArg
){
	LastLocation();
	return CChipTree(std::make_unique<CChipSelfStatus>(CChipSelfStatus::ENERGY), g_pCurField->m_pool);
}

static CChipTree _heat(
	LastLocationArg
){
	LastLocation();
	return CChipTree(std::make_unique<CChipSelfStatus>(CChipSelfStatus::HEAT), g_pCurField->m_pool);
}

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

static CChipTree isRand(
	int num,
	LastLocationArg
){
	LastLocation();
	return CChipTree(std::make_unique<CChipIsRand>(num), g_pCurField->m_pool);
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

	virtual void num_set(int num){m_num = num;}
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

static auto _timeRemained(LastLocationArg) {
	LastLocation();
	return CChipTree(std::make_unique<CChipIfTime>(CChipIfTime::END), g_pCurField->m_pool);
}

//////////////////////////////////////////////////////////////////////////////
// ターゲット機体コード

class CChipIfBodyCode : public CChipCond {
public:
	CChipIfBodyCode(){
		m_Id			= CHIPID_TGT_BODYCODE;
		m_cond_eq		= true;
	}

	virtual ~CChipIfBodyCode(){}

	virtual void num_set(int num){m_bodycode = num;}

	virtual std::string GetLayoutText(void){
		return
			std::format("機体コード={}?", m_bodycode.m_value == 0xFF ? 0 : m_bodycode.get());
	}

	virtual void GetBin(CChipBinary& bin){
		CChipCond::GetBin(bin);
		m_bodycode.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChipCond::SetBin(bin);
		m_bodycode.SetBin(bin);
	}

	ScaledInt<8, 0, 37, 1, 1>	m_bodycode;
};

static auto _bodycode(LastLocationArg) {
	LastLocation();
	return CChipTree(std::make_unique<CChipIfBodyCode>(), g_pCurField->m_pool);
}

//////////////////////////////////////////////////////////////////////////////
// 被ロック数

class CChipIfNumLock : public CChipCond {
public:
	CChipIfNumLock(){
		m_Id			= CHIPID_NUM_LOCKED;
		m_cond_eq		= true;
	}

	virtual ~CChipIfNumLock(){}

	virtual void num_set(int num){m_num = num;}

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
	return CChipTree(std::make_unique<CChipIfNumLock>(), g_pCurField->m_pool);
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

	virtual void num_set(int num){m_num = num;}

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
	return CChipTree(std::make_unique<CChipIfTargetWeaponId>(weapon), g_pCurField->m_pool);
}

//////////////////////////////////////////////////////////////////////////////
// 射線クリア?

class CChipIfLineClear : public CChipCond {
public:
	CChipIfLineClear(){
		m_Id			= CHIPID_IS_LINE_CLEAR;
	}

	virtual ~CChipIfLineClear(){}

	virtual std::string GetLayoutText(void){
		return "射線クリア?";
	}

	virtual void GetBin(CChipBinary& bin){
		CChipCond::GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChipCond::SetBin(bin);
	}
};

static auto _isLineClear(LastLocationArg) {
	LastLocation();
	return CChipTree(std::make_unique<CChipIfLineClear>(), g_pCurField->m_pool);
}

//////////////////////////////////////////////////////////////////////////////
// ターゲット指定

class CChipLockon : public CChip {
public:
	#include "opt_coordinate.h"
	
	CChipLockon(
		int enemy
	){
		m_type		= OKE_ALL;
		m_enemy		= enemy;
		m_Id		= CHIPID_LOCKON;
		
		InitCoordinate();
	}

	virtual ~CChipLockon(){}

	virtual std::string GetLayoutText(void){
		return
			std::format(
				"Lock{}{}\n{}",
				m_EnemyFriendlyStr[m_enemy.get()],
				m_OkeTypeStr[m_type.get()],
				GetCoordinateText()
			);
	}

	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		GetCoordinateBin(bin);
		m_enemy.GetBin(bin);
		m_type.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		SetCoordinateBin(bin);
		m_enemy.SetBin(bin);
		m_type.SetBin(bin);
	}
	
	// option
	auto& type(int param)	{m_type = param;	return *this;}
	
	ScaledInt<>		m_enemy;
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
	return *g_pCurField->m_tree.AddToTree(std::make_unique<CChipLockon>(CChip::ENEMY_FRIENDLY));
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
		"", "からの",
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
	};

	static inline const char *m_StatusTypeStr[] = {
		"静止", "移動", "旋回", "Jmp", "射撃", "格闘", "防御", "特殊", "被弾", "Unlock"
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
				m_SelfTgtTypeStr[m_Id.get() == CHIPID_IF_MY_ACTION ? 0 : 1], m_StatusTypeStr[m_param.get()]
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

static CChipTree is_self_target_status(
	int my_tgt,
	int param
){
	return CChipTree(std::make_unique<CChipTgtAction>(my_tgt, param), g_pCurField->m_pool);
}

#ifndef NO_OKECC_SYNTAX
static CChipTree _isSelfWaiting		(LastLocationArg){LastLocation(); return is_self_target_status(CChip::MY, CChipTgtAction::WAIT);}
static CChipTree _isSelfMoving		(LastLocationArg){LastLocation(); return is_self_target_status(CChip::MY, CChipTgtAction::MOVE);}
static CChipTree _isSelfTurning		(LastLocationArg){LastLocation(); return is_self_target_status(CChip::MY, CChipTgtAction::TURN);}
static CChipTree _isSelfJumping		(LastLocationArg){LastLocation(); return is_self_target_status(CChip::MY, CChipTgtAction::JUMP);}
static CChipTree _isSelfFiring		(LastLocationArg){LastLocation(); return is_self_target_status(CChip::MY, CChipTgtAction::FIRE);}
static CChipTree _isSelfFighting	(LastLocationArg){LastLocation(); return is_self_target_status(CChip::MY, CChipTgtAction::FIGHT);}
static CChipTree _isSelfSpecial		(LastLocationArg){LastLocation(); return is_self_target_status(CChip::MY, CChipTgtAction::SPECIAL);}
static CChipTree _isSelfStumbling	(LastLocationArg){LastLocation(); return is_self_target_status(CChip::MY, CChipTgtAction::STUN);}
static CChipTree _isUnlock			(LastLocationArg){LastLocation(); return is_self_target_status(CChip::MY, CChipTgtAction::UNLOCK);}

static CChipTree _isTargetWaiting	(LastLocationArg){LastLocation(); return is_self_target_status(CChip::TARGET, CChipTgtAction::WAIT);}
static CChipTree _isTargetMoving	(LastLocationArg){LastLocation(); return is_self_target_status(CChip::TARGET, CChipTgtAction::MOVE);}
static CChipTree _isTargetTurning	(LastLocationArg){LastLocation(); return is_self_target_status(CChip::TARGET, CChipTgtAction::TURN);}
static CChipTree _isTargetJumping	(LastLocationArg){LastLocation(); return is_self_target_status(CChip::TARGET, CChipTgtAction::JUMP);}
static CChipTree _isTargetFiring	(LastLocationArg){LastLocation(); return is_self_target_status(CChip::TARGET, CChipTgtAction::FIRE);}
static CChipTree _isTargetFighting	(LastLocationArg){LastLocation(); return is_self_target_status(CChip::TARGET, CChipTgtAction::FIGHT);}
static CChipTree _isTargetSpecial	(LastLocationArg){LastLocation(); return is_self_target_status(CChip::TARGET, CChipTgtAction::SPECIAL);}
static CChipTree _isTargetStumbling	(LastLocationArg){LastLocation(); return is_self_target_status(CChip::TARGET, CChipTgtAction::STUN);}
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

static void lockonPart(int param, LastLocationArg){LastLocation();
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
				"{} = {}",
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
	};

	static inline const char *m_OprStr[] = {
		"=", "+=", "-=", "*=", "/=", "= int", "%=", "= abs", "= max", "= min", "= sqr"
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
		if(-99999.9 > op2 || op2 > 99999.9){
			Error(std::format("Value {} is out of range [-99999.9 - 99999.9]", op2));
		}

		m_op1		= op1;
		m_operator	= opr;
		m_immxvar	= 1;
		m_imm		= (int)std::round(op2 * 10);
		m_op2		= 0;
		m_Id		= CHIPID_CALC;
	}

	virtual ~CChipCalc(){}

	virtual std::string GetLayoutText(void){
		return m_immxvar.get() ?
			std::format("{}{}{}", m_VarNameStr[m_op1.get()], m_OprStr[m_operator.get()], m_imm.get() / 10.0) :
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

CChipVar& CChipVar::operator+=(const double imm) { g_pCurField->m_tree.AddToTree(std::make_unique<CChipCalc>(m_var, CChipCalc::ADD, imm)); return *this; }
CChipVar& CChipVar::operator-=(const double imm) { g_pCurField->m_tree.AddToTree(std::make_unique<CChipCalc>(m_var, CChipCalc::SUB, imm)); return *this; }
CChipVar& CChipVar::operator*=(const double imm) { g_pCurField->m_tree.AddToTree(std::make_unique<CChipCalc>(m_var, CChipCalc::MUL, imm)); return *this; }
CChipVar& CChipVar::operator/=(const double imm) { g_pCurField->m_tree.AddToTree(std::make_unique<CChipCalc>(m_var, CChipCalc::DIV, imm)); return *this; }
CChipVar& CChipVar::operator%=(const double imm) { g_pCurField->m_tree.AddToTree(std::make_unique<CChipCalc>(m_var, CChipCalc::MOD, imm)); return *this; }
CChipVar& CChipVar::operator= (const double imm) { g_pCurField->m_tree.AddToTree(std::make_unique<CChipCalc>(m_var, CChipCalc::MOV, imm)); return *this; }

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
static CChipVal _targetBodycode		(LastLocationArg){LastLocation(); return CChipVal(CChipVal::TGT_BODYCODE);}
static CChipVal _targetActcode		(LastLocationArg){LastLocation(); return CChipVal(CChipVal::TGT_ACTCODE);}
static CChipVal _targetDistance		(LastLocationArg){LastLocation(); return CChipVal(CChipVal::TGT_DISTANCE);}
static CChipVal _targetDistanceXy	(LastLocationArg){LastLocation(); return CChipVal(CChipVal::TGT_DISTANCE_XY);}

static CChipVal mathInt	(double val, LastLocationArg) { LastLocation(); return CChipVal(CChipVal::MATH, std::make_unique<CChipCalc>(0, CChipCalc::INT, val)); }
static CChipVal mathAbs	(double val, LastLocationArg) { LastLocation(); return CChipVal(CChipVal::MATH, std::make_unique<CChipCalc>(0, CChipCalc::ABS, val)); }
static CChipVal mathMax	(double val, LastLocationArg) { LastLocation(); return CChipVal(CChipVal::MATH, std::make_unique<CChipCalc>(0, CChipCalc::MAX, val)); }
static CChipVal mathMin	(double val, LastLocationArg) { LastLocation(); return CChipVal(CChipVal::MATH, std::make_unique<CChipCalc>(0, CChipCalc::MIN, val)); }
static CChipVal mathSqr	(double val, LastLocationArg) { LastLocation(); return CChipVal(CChipVal::MATH, std::make_unique<CChipCalc>(0, CChipCalc::SQR, val)); }

static CChipVal mathInt	(CChipVar& var, LastLocationArg) { LastLocation(); return CChipVal(CChipVal::MATH, std::make_unique<CChipCalc>(0, CChipCalc::INT, var.m_var)); }
static CChipVal mathAbs	(CChipVar& var, LastLocationArg) { LastLocation(); return CChipVal(CChipVal::MATH, std::make_unique<CChipCalc>(0, CChipCalc::ABS, var.m_var)); }
static CChipVal mathMax	(CChipVar& var, LastLocationArg) { LastLocation(); return CChipVal(CChipVal::MATH, std::make_unique<CChipCalc>(0, CChipCalc::MAX, var.m_var)); }
static CChipVal mathMin	(CChipVar& var, LastLocationArg) { LastLocation(); return CChipVal(CChipVal::MATH, std::make_unique<CChipCalc>(0, CChipCalc::MIN, var.m_var)); }
static CChipVal mathSqr	(CChipVar& var, LastLocationArg) { LastLocation(); return CChipVal(CChipVal::MATH, std::make_unique<CChipCalc>(0, CChipCalc::SQR, var.m_var)); }

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
			pchip = std::make_unique<CChipGetStatus>(val.m_type, m_var);
	}

	g_pCurField->m_tree.AddToTree(std::move(pchip));
	return *this;
}

auto& CChipVal::GetCChipCond(void) const {

	std::unique_ptr<CChipCond> pchip;

	if(m_type == CChipVal::TIME){
		pchip = std::make_unique<CChipIfTime>(CChipIfTime::START);
	}else{
		Error("Invalid use of compare operator");
	}
	return *g_pCurField->m_tree.AddToPool(std::move(pchip));
}

CChipTree CChipVal::operator<=(int num) const {return GetCChipCond() <= num;}
CChipTree CChipVal::operator< (int num) const {return GetCChipCond() <  num;}
CChipTree CChipVal::operator>=(int num) const {return GetCChipCond() >= num;}
CChipTree CChipVal::operator> (int num) const {return GetCChipCond() >  num;}
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
		if(-99999.9 > op2 || op2 > 99999.9){
			Error(std::format("Value {} is out of range [-99999.9 - 99999.9]", op2));
		}

		m_op1		= op1;
		m_operator	= opr;
		m_immxvar	= 1;
		m_imm		= (int)std::round(op2 * 10);
		m_op2		= 0;
		m_Id		= CHIPID_IF_CMP;
	}

	virtual ~CChipCmp(){}

	virtual std::string GetLayoutText(void){
		return m_immxvar.get() ?
			std::format("{}{}{}?", m_VarNameStr[m_op1.get()], m_operator_str[m_operator.get()], m_op2.get() / 10.0) :
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
CChipTree CChipVar::operator>=(const CChipVar& op2) const {return CChipTree(std::make_unique<CChipCmp>(m_var, CChip::OP_GE, op2.m_var), g_pCurField->m_pool);}
CChipTree CChipVar::operator<=(const CChipVar& op2) const {return CChipTree(std::make_unique<CChipCmp>(m_var, CChip::OP_LE, op2.m_var), g_pCurField->m_pool);}
CChipTree CChipVar::operator==(const CChipVar& op2) const {return CChipTree(std::make_unique<CChipCmp>(m_var, CChip::OP_EQ, op2.m_var), g_pCurField->m_pool);}
CChipTree CChipVar::operator!=(const CChipVar& op2) const {return !(*this == op2);}
CChipTree CChipVar::operator> (const CChipVar& op2) const {return !(*this <= op2);}
CChipTree CChipVar::operator< (const CChipVar& op2) const {return !(*this >= op2);}

CChipTree CChipVar::operator>=(const double imm) const {return CChipTree(std::make_unique<CChipCmp>(m_var, CChip::OP_GE, imm), g_pCurField->m_pool);}
CChipTree CChipVar::operator<=(const double imm) const {return CChipTree(std::make_unique<CChipCmp>(m_var, CChip::OP_LE, imm), g_pCurField->m_pool);}
CChipTree CChipVar::operator==(const double imm) const {return CChipTree(std::make_unique<CChipCmp>(m_var, CChip::OP_EQ, imm), g_pCurField->m_pool);}
CChipTree CChipVar::operator!=(const double imm) const {return !(*this == imm);}
CChipTree CChipVar::operator> (const double imm) const {return !(*this <= imm);}
CChipTree CChipVar::operator< (const double imm) const {return !(*this >= imm);}
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

	if(
		g_pCurField->GetMode() != CField::BM_IF &&
		g_pCurField->GetMode() != CField::BM_ELSE
	){
		g_pCurField->BlockError("Unexpected endif");
		if(!g_pCurField->m_BlockStack.size()) return;
	}

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

	if(g_pCurField->GetMode() != CField::BM_LOOP){
		g_pCurField->BlockError("Unexpected endloop");
		if(!g_pCurField->m_BlockStack.size()) return;
	}
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
// end

static void okecc_exit(LastLocationArg){
	LastLocation();

	// Goto chip * 2 を置き，1個目を Exit に向ける
	auto chip = std::make_unique<CChipGoto>();

	g_pCurField->m_tree.AddToTree(std::move(chip));
	auto idx = g_pCurField->m_tree.m_LastG;
	g_pCurField->m_tree.AddToTree(std::make_unique<CChipGoto>());

	// 1個目の goto を Exit に向ける
	g_pCurField->m_pool[idx]->m_NextG = IDX_EXIT;
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

#define start_sub(num) if(!start_sub_internal(num)) return; CFieldSwitch FieldInfo(num)

//////////////////////////////////////////////////////////////////////////////
// C との命名被り回避

#ifndef NO_OKECC_SYNTAX
	#define If(cc)		if_statement(cc);
	#define Elseif(cc)	elseif_statement(cc);
	#define Else		else_statement();
	#define Endif		endif_statement();

	#define Break		break_statement()
	#define While(cc)	loop_statement(); If(!(cc)) Break; Endif
	#define Endwhile	endloop_statement();

	#define Return		okecc_exit()

	#define autoTurn				_autoTurn()
	#define bodycode				_bodycode()
	#define energy					_energy()
	#define fight					_fight()
	#define fightHigh				_fightHigh()
	#define fightLong				_fightLong()
	#define fightLow				_fightLow()
	#define health					_health()
	#define heat					_heat()
	#define isLineClear				_isLineClear()
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
	#define isTargetWaiting			_isTargetWaiting()
	#define isUnlock				_isUnlock()
	#define jumpBackward			_jumpBackward()
	#define jumpForward				_jumpForward()
	#define jumpLeft				_jumpLeft()
	#define jumpRight				_jumpRight()
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
	#define numOke					_numOke()
	#define numProjectile			_numProjectile()
	#define stop					_stop()
	#define targetActcode			_targetActcode()
	#define targetAzimuth			_targetAzimuth()
	#define targetBodycode			_targetBodycode()
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
	
	#define	fast					_fast()
	#define wide					_wide()
	#define snipe					_snipe()
	#define target					_target()
	#define off						_off()
#endif
