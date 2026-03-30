#include <algorithm>
#include <concepts>
#include <format>
#include <source_location>
#include <stdexcept>
#include <vector>

#ifdef NO_OKECC_SYNTAX
	#define EXTERN extern
#else
	#define EXTERN
#endif

enum {
	OKE_BIPED,
	OKE_QUADRUPED,
	OKE_VEHICLE,
	OKE_HOVER,
	OKE_FLIGHT,
	OKE_ALL,
};

enum {
	P_BULLET,
	P_MISSILE,
	P_BEAM,
	P_ROCKET,
	P_MINE,
	P_FMINE,
	P_HI_V,
	P_ALL,
};

enum {
	CHIPID_NULL,
	CHIPID_NOP,
	CHIPID_MOVE,
	CHIPID_TURN,
	CHIPID_JUMP,
	CHIPID_ACTION,
	CHIPID_ALTITUDE,
	CHIPID_AMMO,
	CHIPID_FIRE_NEAREST,
	CHIPID_FIRE_FIXED_DIR,
	CHIPID_OPTION,
	CHIPID_DET_OKE,
	CHIPID_DET_BARRIER,
	CHIPID_DET_PROJECTILE,
	CHIPID_MAPPOINT,
	CHIPID_SELF_STATUS,
	CHIPID_SUB,
	CHIPID_RAND,
	CHIPID_TIME,
	CHIPID_LOCKON,
	CHIPID_TGT_DISTANCE,
	CHIPID_TGT_DIR,
	CHIPID_FIRE_TGT,
	CHIPID_IS_COORDINATE,
	CHIPID_IS_ACTION,
	CHIPID_SOUND,
	CHIPID_GET_STATUS,
	CHIPID_GET_COORDINATE,
	CHIPID_GET_TGT_DIR,
	CHIPID_GET_MISC_NUM,
	CHIPID_CALC,
	CHIPID_CLAMP,
	CHIPID_CMP,
	CHIPID_CH_SEND,
	CHIPID_CH_RECV,
	CHIPID_FIRE_COUNTER,
	CHIPID_GOTO,
};

typedef unsigned int UINT;

static constexpr UINT IDX_NONE = 0xFFFFFFFF;
static constexpr UINT IDX_EXIT = 0xFFFFFFFE;

//////////////////////////////////////////////////////////////////////////////

inline std::source_location g_LastLocation = std::source_location::current();
#define LastLocation()	(g_LastLocation = location)
#define LastLocationArg	const std::source_location location = std::source_location::current()

class OkeccError : public std::runtime_error {
public:
	explicit OkeccError(const std::string& message)
		: std::runtime_error(FormatMessage(message)) {}

private:
	std::string FormatMessage(const std::string& message){
		return std::format("{}({}): Error: {}\n", g_LastLocation.file_name(), g_LastLocation.line(), message);
	}
};

//////////////////////////////////////////////////////////////////////////////
// bit scaled int

class CChipBinary {
public:
	CChipBinary(uint32_t val = 0) : m_val(val), m_pos(32){}
	
	uint32_t GetVal(void){return m_val;}
	
	uint32_t	m_val;
	int			m_pos;
};

template<UINT _width, UINT _step = 1, int _offset = 0, bool _sign = false>
class ScaledInt {
public:
	static constexpr UINT width			= _width;
	static constexpr uint32_t bitmask	= (1U << width) - 1;
	static constexpr int offset			= _offset;
	static constexpr UINT step			= _step;
	static constexpr bool sign			= _sign;
	static constexpr int min			= offset - (int)(sign ? (int)(bitmask >> 1) * step : 0);
	static constexpr int max			= offset + (int)(step * (sign ? ((bitmask >> 1) + 1): bitmask));

	void set(int val){
		if(val < min || max < val){
			throw OkeccError(std::format("Value {} is out of range [{} - {}]", val, min, max));
		}

		m_value = ((((val - offset) << 1)/ step + 1) >> 1) & ((1 << width) - 1);
	};

	ScaledInt& operator=(int i){
		set(i);
		return *this;
	}

	int get(void){
		int i = m_value * step + offset;
		return (i > max) ? (i - max * 2) : i;
	}
	
	void GetBin(CChipBinary& bin){
		bin.m_pos -= width;
		bin.m_val |= (m_value & bitmask) << bin.m_pos;
	}
	
	void SetBin(CChipBinary& bin){
		bin.m_pos -= width;
		m_value = (bin.m_val >> bin.m_pos) & bitmask;
	};
	
private:
	UINT	m_value = 0;
};

//////////////////////////////////////////////////////////////////////////////
// Chip class
class CChip {
public:
	enum {
		OP_GE,
		OP_LE,
		OP_EQ,
		OP_NE,
	};
	
	static inline const char *OkeTypeStr[] = {
		"二脚",
		"四脚",
		"車両",
		"ホバー",
		"飛行",
		"全種",
	};
	
	static inline const char *m_operator_str[] = {
		"≧", "≦", "==", "≠"
	};
	
	static inline const char *m_StatusTypeStr[] = {
		"熱量", "燃料", "ダメージ"
	};
	
	enum {
		ENEMY,
		FRIENDLY
	};
	
	static inline const char *m_EnemyFriendlyStr[] = {
		"敵", "味方"
	};
	
	enum {
		SELF,
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
		"A", "B", "C", "D", "E", "F"
	};
	
	CChip(){
		m_Id		= CHIPID_NULL;
	}
	
	virtual ~CChip(){}
	
	virtual std::string GetLayoutText(void){
		return "";
	}
	
	virtual void set_num(int num){}
	virtual void set_operator(int opr){}
	
	bool ValidG(void){return m_NextG < IDX_EXIT;}
	bool ValidR(void){return m_NextR < IDX_EXIT;}
	
	virtual void GetBin(CChipBinary& bin){
		m_Id.GetBin(bin);
		m_RawG.GetBin(bin);
	}
	
	virtual void SetBin(CChipBinary& bin){
		m_Id.SetBin(bin);
		m_RawG.SetBin(bin);
	}
	
	ScaledInt<6>	m_Id;
	ScaledInt<3>	m_RawG;
	ScaledInt<3>	m_RawR;

	UINT			m_NextG = IDX_NONE;
	UINT			m_NextR = IDX_NONE;
};

class CChipCond : public CChip {
public:
	virtual ~CChipCond(){}
	
	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_RawR.GetBin(bin);
	}
	
	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		m_RawR.SetBin(bin);
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
	std::vector<CChip*> m_list;
	UINT m_start = IDX_NONE;	// 開始チップの index
	UINT m_width;
	UINT m_height;

	CChipPool(UINT width, UINT height) : m_width(width), m_height(height) {}

	UINT add(CChip *chip){
		m_list.push_back(chip);
		return (UINT)m_list.size() - 1;
	}

	size_t size(void){return m_list.size();}

	CChip*& operator[](size_t index) {
		return m_list[index];
	}

	CChip* operator[](size_t index) const {
		return m_list[index];
	}

	// Goto を辿って最終地点を取得
	UINT GetFinalDst(UINT idx){
		UINT dst = idx;

		// 最終地点を取得
		while(dst < m_list.size() && m_list[dst]->m_Id.get() == CHIPID_GOTO) dst = m_list[dst]->m_NextG;

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
			if(m_list[u]->m_Id.get() != CHIPID_GOTO){
				m_list[u]->m_NextR = GetFinalDst(m_list[u]->m_NextR);
				m_list[u]->m_NextG = GetFinalDst(m_list[u]->m_NextG);
				IdxNew2Old.push_back(u);
			}else{
				delete m_list[u];
			}
			IdxOld2New.push_back((UINT)IdxNew2Old.size() - 1);
		}
		
		//Goto 削除
		for(UINT u = 0; u < IdxNew2Old.size(); ++u){
			m_list[u] = m_list[IdxNew2Old[u]];
			if(m_list[u]->ValidG()) m_list[u]->m_NextG = IdxOld2New[m_list[u]->m_NextG];
			if(m_list[u]->ValidR()) m_list[u]->m_NextR = IdxOld2New[m_list[u]->m_NextR];
		}
		m_start = IdxOld2New[m_start];
		m_list.resize(IdxNew2Old.size());
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

inline CChipPool	*g_pCurChipPool;

//////////////////////////////////////////////////////////////////////////////
// Chip tree

class CChipTree {
public:

	UINT m_start	= IDX_NONE;	// 開始 chip
	UINT m_LastG	= IDX_NONE;	// 最後の緑矢印を出している chip
	UINT m_LastR	= IDX_NONE; // 最後の赤矢印を出している chip

	// g, r を update
	void AddToG(UINT idx){
		if(m_start == IDX_NONE) m_start = idx;
		if(ValidG()) (*g_pCurChipPool)[m_LastG]->m_NextG = idx;
		m_LastG = idx;
	}

	void AddToR(UINT idx){
		if(m_start == IDX_NONE) m_start = idx;
		if(ValidR()) (*g_pCurChipPool)[m_LastR]->m_NextG = idx;
		m_LastR = idx;
	}

	bool ValidStart(void) const {return m_start < g_pCurChipPool->m_list.size();}
	bool ValidG    (void) const {return m_LastG < g_pCurChipPool->m_list.size();}
	bool ValidR    (void) const {return m_LastR < g_pCurChipPool->m_list.size();}

	// Tree にチップ追加
	CChipTree& add(CChip *pchip){
		UINT idx = g_pCurChipPool->add(pchip);		// チップ追加

		if(ValidStart()){
			(*g_pCurChipPool)[m_LastG]->m_NextG = idx;	// リスト後端に追加したチップをつなげる
		}else{
			m_start = idx;
		}
		m_LastG = idx;

		return *this;
	}
	
	CChipTree operator&&(const CChipTree& b) const {
		CChipTree cc_a = *this;
		CChipTree cc_b = b;
	
		// a.r -> cc_b.start を指す
		cc_a.AddToR(cc_b.m_start);
	
		// False 側: GOTO を生成し a.g, cc_b.g, tree.g はそれを指す
		UINT idx = g_pCurChipPool->add(new CChipGoto);
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
		UINT idx = g_pCurChipPool->add(new CChipGoto);
		cc_a.AddToR(idx);
		cc_b.AddToR(idx);
	
		// False 側: tree.g は cc_b.g になる
		cc_a.m_LastG = cc_b.m_LastG;
	
		return cc_a;
	}
	
	CChipTree operator!(void) const {
		CChipTree tree = CChipTree();
		
		tree.m_start = m_start;
		tree.m_LastG = m_LastR;
		tree.m_LastR = m_LastG;
		
		return tree;
	}
};

inline CChipTree *g_pCurTree;

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

class CCond {
public:
	
	CCond(CChip *pchip) : m_pchip(pchip){}
	
	CChip *m_pchip;
	
	CChipTree GetCChipTree(void) const {
		CChipTree tree;
		
		// チップ単体からツリーに変換 (Condition chip)
		// R 側に Goto を足して，常に m_NextG を触ればいいようにする
		tree.m_start = 
		tree.m_LastG = g_pCurChipPool->add(m_pchip);
		
		(*g_pCurChipPool)[tree.m_start]->m_NextR =
		tree.m_LastR = g_pCurChipPool->add(new CChipGoto);
		
		return tree;
	}
	
	operator CChipTree() const {
		return GetCChipTree();
	}
	
	CChipTree operator!(void) const {
		return !GetCChipTree();
	}
	
	CChipTree operator>=(int num) const {
		m_pchip->set_num(num);
		m_pchip->set_operator(CChip::OP_GE);
		return GetCChipTree();
	}

	CChipTree operator>(int num) const {
		return *this >= (num - 1);
	}

	CChipTree operator<=(int num) const {
		m_pchip->set_num(num);
		m_pchip->set_operator(CChip::OP_LE);
		return GetCChipTree();
	}

	CChipTree operator<(int num) const {
		return *this <= (num + 1);
	}
};

//////////////////////////////////////////////////////////////////////////////

class CChipVal {
public:
	enum {
		WEAPON1,
		WEAPON2,
		WEAPON3,
		WEAPON4,
		WEAPON5,
		HEAT,
		FUEL,
		DAMAGE,
		TIME,
		FRIENDLY,
		ENEMY,
		RAND,
		CH_RECV,
		SELF_POS_X,
		SELF_POS_Y,
		SELF_POS_Z,
		TGT_POS_X,
		TGT_POS_Y,
		TGT_POS_Z,
	};
	
	CChipVal(UINT type, int param = 0) : m_type(type), m_param(param){}
	
	CChipTree operator!(void) const {
		return !(*this >= 1);
	}
	
	operator CChipTree() const {
		return *this >= 1;
	}
	
	CCond GetCChipCond(void) const;
	CChipTree operator<=(int num) const;
	CChipTree operator< (int num) const;
	CChipTree operator>=(int num) const;
	CChipTree operator> (int num) const;
	
	UINT m_type;
	int m_param;
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
	
	CChipVar& operator+=(const int op2);
	CChipVar& operator-=(const int op2);
	CChipVar& operator*=(const int op2);
	CChipVar& operator/=(const int op2);
	CChipVar& operator%=(const int op2);
	CChipVar& operator= (const int op2);
	
	CChipTree operator>=(const CChipVar& op2) const;
	CChipTree operator<=(const CChipVar& op2) const;
	CChipTree operator==(const CChipVar& op2) const;
	CChipTree operator!=(const CChipVar& op2) const;
	CChipTree operator> (const CChipVar& op2) const;
	CChipTree operator< (const CChipVar& op2) const;
	
	CChipTree operator>=(const int imm) const;
	CChipTree operator<=(const int imm) const;
	CChipTree operator==(const int imm) const;
	CChipTree operator!=(const int imm) const;
	CChipTree operator> (const int imm) const;
	CChipTree operator< (const int imm) const;
};

inline CChipVar A(0);
inline CChipVar B(1);
inline CChipVar C(2);
inline CChipVar D(3);
inline CChipVar E(4);
inline CChipVar F(5);

//////////////////////////////////////////////////////////////////////////////
// NOP

static constexpr UINT NOP_AE = 62;

class CChipNop : public CChip {
public:
	CChipNop(UINT param = 0){
		m_Id	= CHIPID_NOP;
		m_param	= param;
	}

	virtual ~CChipNop(){}
	
	virtual std::string GetLayoutText(void){
		return
			m_param.get() == 0 ? "NOP" :
			m_param.get() == NOP_AE ? "Wait AE" :
			std::format("Wait {:2d}", m_param.get());
	}
	
	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_param.GetBin(bin);
	}
	
	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		m_param.SetBin(bin);
	}
	
	ScaledInt<5,2> m_param;
};

static void wait(
	UINT param,
	LastLocationArg
){
	LastLocation();
	g_pCurTree->add(new CChipNop(param));
}

static void nop    (LastLocationArg){wait(0,      location);}
static void wait_ae(LastLocationArg){wait(NOP_AE, location);}

//////////////////////////////////////////////////////////////////////////////
// move

class CChipMove : public CChip {
public:
	enum {
		FWD,
		BACK,
		RIGHT,
		LEFT,
		STOP,
	};

	CChipMove(UINT param){
		m_Id	= CHIPID_MOVE;
		m_param	= param;
	}

	virtual ~CChipMove(){}

	virtual std::string GetLayoutText(void){
		return
			m_param.get() == FWD	? "前進" :
			m_param.get() == BACK	? "後退" :
			m_param.get() == RIGHT	? "右移動" :
			m_param.get() == LEFT	? "左移動" :
									  "停止";
	}
	
	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_pad.GetBin(bin);
		m_param.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		m_pad.SetBin(bin);
		m_param.SetBin(bin);
	}
	
	ScaledInt<1> m_pad;
	ScaledInt<3> m_param;
};

static void put_move_chip(UINT param){
	g_pCurTree->add(new CChipMove(param));
}

static void move_forward (LastLocationArg){LastLocation(); put_move_chip(CChipMove::FWD);}
static void move_backward(LastLocationArg){LastLocation(); put_move_chip(CChipMove::BACK);}
static void move_left    (LastLocationArg){LastLocation(); put_move_chip(CChipMove::LEFT);}
static void move_right   (LastLocationArg){LastLocation(); put_move_chip(CChipMove::RIGHT);}
static void stop         (LastLocationArg){LastLocation(); put_move_chip(CChipMove::STOP);}

//////////////////////////////////////////////////////////////////////////////
// rotate

class CChipTurn : public CChip {
public:
	enum {
		RIGHT,
		LEFT,
	};

	CChipTurn(UINT param){
		m_Id	= CHIPID_TURN;
		m_param	= param;
	}

	virtual ~CChipTurn(){}

	virtual std::string GetLayoutText(void){
		return m_param.get() == LEFT	? "左旋回" : "右旋回";
	}
	
	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_param.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		m_param.SetBin(bin);
	}

	ScaledInt<1> m_param;
};

static void put_turn_chip(UINT param){
	g_pCurTree->add(new CChipTurn(param));
}

static void turn_left (LastLocationArg){LastLocation(); put_turn_chip(CChipTurn::LEFT);}
static void turn_right(LastLocationArg){LastLocation(); put_turn_chip(CChipTurn::RIGHT);}

//////////////////////////////////////////////////////////////////////////////
// Jump

class CChipJump : public CChip {
public:
	enum {
		FWD,
		BACK,
		RIGHT,
		LEFT,
	};

	CChipJump(UINT param){
		m_Id	= CHIPID_JUMP;
		m_param	= param;
	}

	virtual ~CChipJump(){}

	virtual std::string GetLayoutText(void){
		return
			m_param.get() == FWD	? "前Jmp" :
			m_param.get() == BACK	? "後Jmp" :
			m_param.get() == LEFT	? "左Jmp" : "右Jmp";
	}
	
	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_param.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		m_param.SetBin(bin);
	}

	ScaledInt<2> m_param;
};

static void put_jump_chip(UINT param){
	g_pCurTree->add(new CChipJump(param));
}

static void jump_forward (LastLocationArg){LastLocation(); put_jump_chip(CChipJump::FWD);}
static void jump_backward(LastLocationArg){LastLocation(); put_jump_chip(CChipJump::BACK);}
static void jump_left    (LastLocationArg){LastLocation(); put_jump_chip(CChipJump::LEFT);}
static void jump_right   (LastLocationArg){LastLocation(); put_jump_chip(CChipJump::RIGHT);}

//////////////////////////////////////////////////////////////////////////////
// Action

class CChipAction : public CChip {
public:
	enum {
		FIGHT,
		CROUCH,
		GUARD,
		ACTION1,
		ACTION2,
		ACTION3,
	};

	CChipAction(UINT param){
		m_Id	= CHIPID_ACTION;
		m_param	= param;
	}

	virtual ~CChipAction(){}

	virtual std::string GetLayoutText(void){
		return
			m_param.get() == FIGHT		? "格闘" :
			m_param.get() == CROUCH		? "伏せ" :
			m_param.get() == GUARD		? "防御" :
			m_param.get() == ACTION1	? "Action1" :
			m_param.get() == ACTION2	? "Action2" : "Action3";
	}
	
	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_param.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		m_param.SetBin(bin);
	}

	ScaledInt<3> m_param;
};

static void put_action_chip(UINT param){
	g_pCurTree->add(new CChipAction(param));
}

static void fight (LastLocationArg){LastLocation(); put_action_chip(CChipAction::FIGHT);}
static void crouch(LastLocationArg){LastLocation(); put_action_chip(CChipAction::CROUCH);}
static void guard (LastLocationArg){LastLocation(); put_action_chip(CChipAction::GUARD);}
static void action(UINT param, LastLocationArg){LastLocation(); put_action_chip(CChipAction::ACTION1 + param - 1);}

//////////////////////////////////////////////////////////////////////////////
// alt

class CChipAlt : public CChip {
public:
	enum {
		LOW,
		MID,
		HIGH,
	};

	CChipAlt(UINT param){
		m_Id	= CHIPID_ALTITUDE;
		m_param	= param;
	}

	virtual ~CChipAlt(){}

	virtual std::string GetLayoutText(void){
		return
			m_param.get() == HIGH	? "高高度" :
			m_param.get() == MID	? "中高度" : "低高度";
	}
	
	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_param.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		m_param.SetBin(bin);
	}

	ScaledInt<2> m_param;
};

static void put_alt_chip(UINT param){
	g_pCurTree->add(new CChipAlt(param));
}

static void move_high(LastLocationArg){LastLocation(); put_alt_chip(CChipAlt::HIGH);}
static void move_mid (LastLocationArg){LastLocation(); put_alt_chip(CChipAlt::MID);}
static void move_low (LastLocationArg){LastLocation(); put_alt_chip(CChipAlt::LOW);}

//////////////////////////////////////////////////////////////////////////////
// fire

class CChipFireNearest : public CChip {
public:
	CChipFireNearest(
		int angleCenter,
		int angleRange,
		int distance,
		int type,
		int weapon,
		int cnt
	){
		m_Id			= CHIPID_FIRE_NEAREST;
		m_angleCenter	= angleCenter;
		m_angleRange	= angleRange;
		m_distance		= distance;
		m_type			= type;
		m_weapon		= weapon;
		m_cnt			= cnt;
	}

	virtual ~CChipFireNearest(){}

	virtual std::string GetLayoutText(void){
		return
			std::format(
				"射撃 #{}x{}\n{} {}m\n {},{}",
				m_weapon.get(),
				m_cnt.get(),
				OkeTypeStr[m_type.get()], m_distance.get(),
				m_angleCenter.get(), m_angleRange.get()
			);
	}
	
	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_angleCenter.GetBin(bin);
		m_angleRange.GetBin(bin);
		m_distance.GetBin(bin);
		m_type.GetBin(bin);
		m_weapon.GetBin(bin);
		m_cnt.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		m_angleCenter.SetBin(bin);
		m_angleRange.SetBin(bin);
		m_distance.SetBin(bin);
		m_type.SetBin(bin);
		m_weapon.SetBin(bin);
		m_cnt.SetBin(bin);
	}

	ScaledInt<5, 16, 0, true>	m_angleCenter;
	ScaledInt<4, 32, 32>		m_angleRange;
	ScaledInt<4, 20, 20>		m_distance;
	ScaledInt<3>				m_type;
	ScaledInt<3, 1, 1>			m_weapon;
	ScaledInt<3, 1, 1>			m_cnt;
};

static void fire(
	int angleCenter,
	int angleRange,
	int distance,
	int type,
	int weapon,
	int cnt,
	LastLocationArg
){
	LastLocation();
	g_pCurTree->add(new CChipFireNearest(
		angleCenter,
		angleRange,
		distance,
		type,
		weapon,
		cnt
	));
}

class CChipFireFixedDir : public CChip {
public:
	CChipFireFixedDir(
		int direction,
		int elevation,
		int weapon,
		int cnt
	){
		m_Id		= CHIPID_FIRE_FIXED_DIR;
		m_direction	= direction;
		m_elevation	= -elevation;
		m_weapon	= weapon;
		m_cnt		= cnt;
	}

	virtual ~CChipFireFixedDir(){}

	virtual std::string GetLayoutText(void){
		return
			std::format(
				"射撃 #{}x{}\n方位 {}\n角度 {}",
				m_weapon.get(),
				m_cnt.get(),
				m_direction.get(), -m_elevation.get()
			);
	}
	
	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_direction.GetBin(bin);
		m_elevation.GetBin(bin);
		m_weapon.GetBin(bin);
		m_cnt.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		m_direction.SetBin(bin);
		m_elevation.SetBin(bin);
		m_weapon.SetBin(bin);
		m_cnt.SetBin(bin);
	}

	ScaledInt<5, 16, 0, true>	m_direction;
	ScaledInt<4, 16, -112>		m_elevation;
	ScaledInt<3, 1, 1>			m_weapon;
	ScaledInt<3, 1, 1>			m_cnt;
};

static void fire_direction(
	int direction,
	int elevation,
	int weapon,
	int cnt,
	LastLocationArg
){
	LastLocation();
	g_pCurTree->add(new CChipFireFixedDir(
		direction,
		elevation,
		weapon,
		cnt
	));
}

class CChipFireTarget : public CChip {
public:
	CChipFireTarget(
		int weapon,
		int cnt
	){
		m_Id		= CHIPID_FIRE_TGT;
		m_weapon	= weapon;
		m_cnt		= cnt;
	}

	virtual ~CChipFireTarget(){}

	virtual std::string GetLayoutText(void){
		return
			std::format(
				"TGT射撃\n#{}x{}",
				m_weapon.get(),
				m_cnt.get()
			);
	}
	
	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_weapon.GetBin(bin);
		m_cnt.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		m_weapon.SetBin(bin);
		m_cnt.SetBin(bin);
	}

	ScaledInt<3, 1, 1>		m_weapon;
	ScaledInt<3, 1, 1>		m_cnt;
};

static void fire_target(
	int weapon,
	int cnt,
	LastLocationArg
){
	LastLocation();
	g_pCurTree->add(new CChipFireTarget(
		weapon,
		cnt
	));
}

//////////////////////////////////////////////////////////////////////////////
// カウンタ方位射撃

class CChipFireCounter : public CChip {
public:
	CChipFireCounter(
		int direction,
		int elevation,
		int weapon,
		int cnt
	){
		m_Id		= CHIPID_FIRE_COUNTER;
		m_direction	= direction;
		m_elevation	= elevation;
		m_weapon	= weapon;
		m_cnt		= cnt;
	}

	virtual ~CChipFireCounter(){}

	virtual std::string GetLayoutText(void){
		return
			std::format(
				"射撃 #{}x{}\n方位 {}\n角度 {}",
				m_weapon.get(),
				m_cnt.get(),
				m_VarNameStr[m_direction.get()], m_VarNameStr[m_elevation.get()]
			);
	}
	
	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_direction.GetBin(bin);
		m_elevation.GetBin(bin);
		m_weapon.GetBin(bin);
		m_cnt.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		m_direction.SetBin(bin);
		m_elevation.SetBin(bin);
		m_weapon.SetBin(bin);
		m_cnt.SetBin(bin);
	}

	ScaledInt<3>		m_direction;
	ScaledInt<3>		m_elevation;
	ScaledInt<3, 1, 1>	m_weapon;
	ScaledInt<3, 1, 1>	m_cnt;
};

static void fire_direction(
	CChipVar& direction,
	CChipVar& elevation,
	int weapon,
	int cnt,
	LastLocationArg
){
	LastLocation();
	g_pCurTree->add(new CChipFireCounter(
		direction.m_var,
		elevation.m_var,
		weapon,
		cnt
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
		return std::format("Option #{}", m_param.get());
	}
	
	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_param.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		m_param.SetBin(bin);
	}

	ScaledInt<2, 1, 1> m_param;
};

static void option(UINT param, LastLocationArg){
	LastLocation();
	g_pCurTree->add(new CChipOption(param));
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
		m_Id		= CHIPID_AMMO;
	}
	
	virtual ~CChipAmmoNum(){}

	virtual void set_num(int num){m_num = num;}
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
		m_weapon.GetBin(bin);
		m_num.GetBin(bin);
		m_operator.GetBin(bin);
	}
	
	virtual void SetBin(CChipBinary& bin){
		CChipCond::SetBin(bin);
		m_weapon.SetBin(bin);
		m_num.SetBin(bin);
		m_operator.SetBin(bin);
	}
	
	ScaledInt<3>		m_weapon;
	ScaledInt<7>		m_num;
	ScaledInt<1>		m_operator;
};

static CCond option_num(
	int weapon,
	LastLocationArg
){
	LastLocation();
	return CCond(new CChipAmmoNum(weapon + 4));
}

//////////////////////////////////////////////////////////////////////////////
// 近くの OKE を探索

class CChipOkeNum : public CChipCond{
public:
	CChipOkeNum(
		int angleCenter,
		int angleRange,
		int distance,
		int type,
		int enemy
	){
		m_angleCenter	= angleCenter;
		m_angleRange	= angleRange;
		m_distance		= distance;
		m_type			= type;
		m_enemy			= enemy;
		m_operator		= OP_GE;
		m_num			= 1;
		m_Id			= CHIPID_DET_OKE;
	}
	
	virtual ~CChipOkeNum(){}

	virtual void set_num(int num){m_num = num;}
	virtual void set_operator(int opr){m_operator = opr;}
	
	virtual std::string GetLayoutText(void){
		return
			std::format(
				"{} {}m\n{}{}{}?\n{},{}",
				m_EnemyFriendlyStr[m_enemy.get()], m_distance.get(),
				OkeTypeStr[m_type.get()], m_operator_str[m_operator.get()], m_num.get(),
				m_angleCenter.get(), m_angleRange.get()
			);
	}
	
	virtual void GetBin(CChipBinary& bin){
		CChipCond::GetBin(bin);
		m_angleCenter.GetBin(bin);
		m_angleRange.GetBin(bin);
		m_distance.GetBin(bin);
		m_enemy.GetBin(bin);
		m_type.GetBin(bin);
		m_num.GetBin(bin);
		m_operator.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChipCond::SetBin(bin);
		m_angleCenter.SetBin(bin);
		m_angleRange.SetBin(bin);
		m_distance.SetBin(bin);
		m_enemy.SetBin(bin);
		m_type.SetBin(bin);
		m_num.SetBin(bin);
		m_operator.SetBin(bin);
	}

	ScaledInt<5, 16, 0, true>	m_angleCenter;
	ScaledInt<4, 32, 32>		m_angleRange;
	ScaledInt<4, 20, 20>		m_distance;
	ScaledInt<1>		 		m_enemy;
	ScaledInt<3>				m_type;
	ScaledInt<2, 1, 1>			m_num;
	ScaledInt<1>				m_operator;
};

static CCond oke_num(
	int angleCenter,
	int angleRange,
	int distance,
	int type,
	int enemy
){
	return CCond(new CChipOkeNum(angleCenter, angleRange, distance, type, enemy));
}

static CCond enemy_num(
	int angleCenter,
	int angleRange,
	int distance,
	int type,
	LastLocationArg
){
	LastLocation();
	return oke_num(angleCenter, angleRange, distance, type, CChip::ENEMY);
}

static CCond friendly_num(
	int angleCenter,
	int angleRange,
	int distance,
	int type,
	LastLocationArg
){
	LastLocation();
	return oke_num(angleCenter, angleRange, distance, type, CChip::FRIENDLY);
}

//////////////////////////////////////////////////////////////////////////////
// 近くの障害物を探索

class CChipBarrier : public CChipCond {
public:

	CChipBarrier(
		int angleCenter,
		int angleRange,
		int distance,
		int opr		= OP_GE,
		int num		= 3
	){
		m_angleCenter	= angleCenter;
		m_angleRange	= angleRange;
		m_distance		= distance;
		m_operator		= opr;
		m_num			= num;
		m_Id			= CHIPID_DET_BARRIER;
	}
	
	virtual ~CChipBarrier(){}

	virtual void set_num(int num){m_num = num;}
	virtual void set_operator(int opr){m_operator = opr;}
	
	virtual std::string GetLayoutText(void){
		return
			std::format(
				"障害物\n高さ{}{}m?\n{}m\n{},{}",
				m_operator_str[m_operator.get()], m_num.get(),
				m_distance.get(),
				m_angleCenter.get(), m_angleRange.get()
			);
	}
	
	virtual void GetBin(CChipBinary& bin){
		CChipCond::GetBin(bin);
		m_angleCenter.GetBin(bin);
		m_angleRange.GetBin(bin);
		m_distance.GetBin(bin);
		m_num.GetBin(bin);
		m_operator.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChipCond::SetBin(bin);
		m_angleCenter.SetBin(bin);
		m_angleRange.SetBin(bin);
		m_distance.SetBin(bin);
		m_num.SetBin(bin);
		m_operator.SetBin(bin);
	}

	ScaledInt<5, 16, 0, true>	m_angleCenter;
	ScaledInt<4, 32, 32>		m_angleRange;
	ScaledInt<4, 10, 10>		m_distance;
	ScaledInt<3, 3, 3>			m_num;
	ScaledInt<1>				m_operator;
};

static CCond is_barrier_over(
	int angleCenter,
	int angleRange,
	int distance,
	int height,
	LastLocationArg
){
	LastLocation();
	return CCond(new CChipBarrier(angleCenter, angleRange, distance, CChip::OP_GE, height));
}

static CCond is_barrier_under(
	int angleCenter,
	int angleRange,
	int distance,
	int height,
	LastLocationArg
){
	LastLocation();
	return CCond(new CChipBarrier(angleCenter, angleRange, distance, CChip::OP_LE, height));
}

//////////////////////////////////////////////////////////////////////////////
// 近くの危険物を探索

class CChipProjectileNum : public CChipCond {
public:
	static inline const char *m_ProjectileTypeStr[] = {
		"弾丸", "ミサイル", "ビーム", "ロケット", "地雷", "機雷", "高速", "発射物"
	};
	
	CChipProjectileNum(
		int angleCenter,
		int angleRange,
		int distance,
		int type
	){
		m_angleCenter	= angleCenter;
		m_angleRange	= angleRange;
		m_distance		= distance;
		m_type			= type;
		m_operator		= OP_GE;
		m_num			= 3;
		m_Id			= CHIPID_DET_PROJECTILE;
	}
	
	virtual ~CChipProjectileNum(){}

	virtual void set_num(int num){m_num = num;}
	virtual void set_operator(int opr){m_operator = opr;}
	
	virtual std::string GetLayoutText(void){
		return
			std::format(
				"{} {}m\n{}{}?\n{},{}",
				m_ProjectileTypeStr[m_type.get()], m_distance.get(),
				m_operator_str[m_operator.get()], m_num.get(),
				m_angleCenter.get(), m_angleRange.get()
			);
	}
	
	virtual void GetBin(CChipBinary& bin){
		CChipCond::GetBin(bin);
		m_angleCenter.GetBin(bin);
		m_angleRange.GetBin(bin);
		m_distance.GetBin(bin);
		m_type.GetBin(bin);
		m_num.GetBin(bin);
		m_operator.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChipCond::SetBin(bin);
		m_angleCenter.SetBin(bin);
		m_angleRange.SetBin(bin);
		m_distance.SetBin(bin);
		m_type.SetBin(bin);
		m_num.SetBin(bin);
		m_operator.SetBin(bin);
	}

	ScaledInt<5, 16, 0, true>	m_angleCenter;
	ScaledInt<4, 32, 32>		m_angleRange;
	ScaledInt<4, 10, 10>		m_distance;
	ScaledInt<3>				m_type;
	ScaledInt<3>				m_num;
	ScaledInt<1>				m_operator;
};

static CCond projectile_num(
	int angleCenter,
	int angleRange,
	int distance,
	int type,
	LastLocationArg
){
	LastLocation();
	return CCond(new CChipProjectileNum(angleCenter, angleRange, distance, type));
}

//////////////////////////////////////////////////////////////////////////////
// マップポイント

class CChipMapPoint : public CChipCond {
public:
	CChipMapPoint(
		int angleCenter,
		int angleRange,
		int distance,
		int map_x,
		int map_y
	){
		m_angleCenter	= angleCenter;
		m_angleRange	= angleRange;
		m_distance		= distance;
		m_x				= map_x;
		m_y				= map_y;
		m_Id			= CHIPID_MAPPOINT;
	}
	
	virtual ~CChipMapPoint(){}

	virtual std::string GetLayoutText(void){
		return std::format(
			"MAP[{}{}]\n{}m?\n{},{}",
			m_x.get(), std::string(1, 'A' + m_y.get()),
			m_distance.get(),
			m_angleCenter.get(), m_angleRange.get()
		);
	}
	
	virtual void GetBin(CChipBinary& bin){
		CChipCond::GetBin(bin);
		m_angleCenter.GetBin(bin);
		m_angleRange.GetBin(bin);
		m_distance.GetBin(bin);
		m_x.GetBin(bin);
		m_y.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChipCond::SetBin(bin);
		m_angleCenter.SetBin(bin);
		m_angleRange.SetBin(bin);
		m_distance.SetBin(bin);
		m_x.SetBin(bin);
		m_y.SetBin(bin);
	}

	ScaledInt<5, 16, 0, true>	m_angleCenter;
	ScaledInt<4, 32, 32>		m_angleRange;
	ScaledInt<4, 20, 20>		m_distance;
	ScaledInt<3, 1, 1>			m_x;
	ScaledInt<3, 1, 1>			m_y;
};

static CCond is_mappoint(
	int angleCenter,
	int angleRange,
	int distance,
	int map_x,
	int map_y,
	LastLocationArg
){
	LastLocation();
	return CCond(new CChipMapPoint(angleCenter, angleRange, distance, map_x, map_y));
}

//////////////////////////////////////////////////////////////////////////////
// 自機の状態確認

class CChipSelfStatus : public CChipCond {
public:
	enum {
		HEAT,
		FUEL,
		DAMAGE,
	};
	
	CChipSelfStatus(
		int type
	){
		m_type			= type;
		m_operator		= OP_GE;
		m_num			= 1;
		m_Id			= CHIPID_SELF_STATUS;
	}
	
	virtual ~CChipSelfStatus(){}

	virtual void set_num(int num){m_num = num;}
	virtual void set_operator(int opr){m_operator = opr;}
	
	virtual std::string GetLayoutText(void){
		return
			std::format(
				"{}{}{}?",
				m_StatusTypeStr[m_type.get()],
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

	ScaledInt<2>			m_type;
	ScaledInt<5, 5>			m_num;
	ScaledInt<1>			m_operator;
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

	ScaledInt<1,1,1> m_no;
};

static void sub(
	int no,
	LastLocationArg
){
	LastLocation();
	g_pCurTree->add(new CChipSubroutine(no));
}

//////////////////////////////////////////////////////////////////////////////
// 乱数

class CChipIsRand : public CChipCond {
public:
	
	CChipIsRand(
		int num1,
		int num2
	){
		m_Id	= CHIPID_RAND;
		m_num1	= num1;
		m_num2	= num2;
	}
	
	virtual ~CChipIsRand(){}
	
	virtual std::string GetLayoutText(void){
		return
			std::format(
				"乱数\n{}/{}?",
				m_num1.get(), m_num2.get()
			);
	}
	
	virtual void GetBin(CChipBinary& bin){
		CChipCond::GetBin(bin);
		m_num1.GetBin(bin);
		m_num2.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChipCond::SetBin(bin);
		m_num1.SetBin(bin);
		m_num2.SetBin(bin);
	}

	ScaledInt<6, 1, 1>		m_num1;
	ScaledInt<6, 1, 1>		m_num2;
};

static CChipTree is_rand(
	int num1,
	int num2,
	LastLocationArg
){
	LastLocation();
	return CCond(new CChipIsRand(num1, num2)).GetCChipTree();
}

//////////////////////////////////////////////////////////////////////////////
// 時間

class CChipTime : public CChipCond {
public:
	
	CChipTime(void){
		m_operator		= OP_GE;
		m_num			= 1;
		m_Id			= CHIPID_TIME;
	}
	
	virtual ~CChipTime(){}

	virtual void set_num(int num){m_num = num;}
	virtual void set_operator(int opr){m_operator = opr;}
	
	virtual std::string GetLayoutText(void){
		return
			std::format(
				"時間{}{}?",
				m_operator_str[m_operator.get()], m_num.get()
			);
	}
	
	virtual void GetBin(CChipBinary& bin){
		CChipCond::GetBin(bin);
		m_num.GetBin(bin);
		m_operator.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChipCond::SetBin(bin);
		m_num.SetBin(bin);
		m_operator.SetBin(bin);
	}

	ScaledInt<6, 5>			m_num;
	ScaledInt<1>			m_operator;
};

//////////////////////////////////////////////////////////////////////////////
// ターゲット指定

class CChipLockon : public CChip{
public:
	CChipLockon(
		int angleCenter,
		int angleRange,
		int distance,
		int type,
		int enemy
	){
		m_angleCenter	= angleCenter;
		m_angleRange	= angleRange;
		m_distance		= distance;
		m_type			= type;
		m_enemy			= enemy;
		m_Id			= CHIPID_LOCKON;
	}
	
	virtual ~CChipLockon(){}

	virtual std::string GetLayoutText(void){
		return
			std::format(
				"Lock\n{} {}m\n{},{}",
				m_EnemyFriendlyStr[m_enemy.get()], m_distance.get(),
				m_angleCenter.get(), m_angleRange.get()
			);
	}
	
	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_angleCenter.GetBin(bin);
		m_angleRange.GetBin(bin);
		m_distance.GetBin(bin);
		m_enemy.GetBin(bin);
		m_type.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		m_angleCenter.SetBin(bin);
		m_angleRange.SetBin(bin);
		m_distance.SetBin(bin);
		m_enemy.SetBin(bin);
		m_type.SetBin(bin);
	}

	ScaledInt<5, 16, 0, true>	m_angleCenter;
	ScaledInt<4, 32, 32>		m_angleRange;
	ScaledInt<4, 20, 20>		m_distance;
	ScaledInt<1>				m_enemy;
	ScaledInt<3>				m_type;
};

static void lockon(
	int angleCenter,
	int angleRange,
	int distance,
	int type,
	LastLocationArg
){
	LastLocation();
	g_pCurTree->add(new CChipLockon(
		angleCenter,
		angleRange,
		distance,
		type,
		CChip::ENEMY
	));
}

//////////////////////////////////////////////////////////////////////////////
// ターゲット距離

class CChipTgtDistance : public CChipCond {
public:
	
	CChipTgtDistance(void){
		m_operator		= OP_GE;
		m_num			= 1;
		m_Id			= CHIPID_TGT_DISTANCE;
	}
	
	virtual ~CChipTgtDistance(){}

	virtual void set_num(int num){m_num = num;}
	virtual void set_operator(int opr){m_operator = opr;}
	
	virtual std::string GetLayoutText(void){
		return
			std::format(
				"TGT\n距離{}{}m?",
				m_operator_str[m_operator.get()], m_num.get()
			);
	}
	
	virtual void GetBin(CChipBinary& bin){
		CChipCond::GetBin(bin);
		m_num.GetBin(bin);
		m_operator.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChipCond::SetBin(bin);
		m_num.SetBin(bin);
		m_operator.SetBin(bin);
	}

	ScaledInt<6, 5>		m_num;
	ScaledInt<1>		m_operator;
};

static CCond target_distance(
	LastLocationArg
){
	LastLocation();
	return CCond(new CChipTgtDistance());
}

//////////////////////////////////////////////////////////////////////////////
// ターゲット方向

class CChipTgtDirection : public CChipCond {
public:
	
	CChipTgtDirection(
		int angleCenter,
		int angleRange
	){
		m_angleCenter	= angleCenter;
		m_angleRange	= angleRange;
		m_Id			= CHIPID_TGT_DIR;
	}
	
	virtual ~CChipTgtDirection(){}

	virtual std::string GetLayoutText(void){
		return
			std::format(
				"TGT方向\n{},{}?",
				m_angleCenter.get(), m_angleRange.get()
			);
	}
	
	virtual void GetBin(CChipBinary& bin){
		CChipCond::GetBin(bin);
		m_angleCenter.GetBin(bin);
		m_angleRange.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChipCond::SetBin(bin);
		m_angleCenter.SetBin(bin);
		m_angleRange.SetBin(bin);
	}

	ScaledInt<5, 16, 0, true>	m_angleCenter;
	ScaledInt<4, 32, 32>		m_angleRange;
};

static CChipTree is_target_direction(
	int angleCenter,
	int angleRange,
	LastLocationArg
){
	LastLocation();
	return CCond(new CChipTgtDirection(angleCenter, angleRange)).GetCChipTree();
}

//////////////////////////////////////////////////////////////////////////////
// ターゲット方向

class CChipGetTgtDirection : public CChip {
public:
	
	CChipGetTgtDirection(
		int var_dir,
		int var_elev
	){
		m_var_dir		= var_dir;
		m_var_elev		= var_elev;
		m_Id			= CHIPID_GET_TGT_DIR;
	}
	
	virtual ~CChipGetTgtDirection(){}

	virtual std::string GetLayoutText(void){
		return
			std::format(
				"TGT方向\n{} = 方向\n{} = 仰角",
				m_VarNameStr[m_var_dir.get()],
				m_VarNameStr[m_var_elev.get()]
			);
	}
	
	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_var_dir.GetBin(bin);
		m_var_elev.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		m_var_dir.SetBin(bin);
		m_var_elev.SetBin(bin);
	}

	ScaledInt<3>	m_var_dir;
	ScaledInt<3>	m_var_elev;
};

static void get_target_direction(
	CChipVar var_dir,
	CChipVar var_elev,
	LastLocationArg
){
	LastLocation();
	g_pCurTree->add(new CChipGetTgtDirection(var_dir.m_var, var_elev.m_var));
}

/////////////////////////////////////////////////////////////////////////////////////////////
// ターゲット XYZ 座標

class CChipIsCoordinate : public CChipCond{
public:
	
	CChipIsCoordinate(
		int self_tgt,
		int param
	){
		m_self_tgt		= self_tgt;
		m_param			= param;
		m_operator		= OP_GE;
		m_num			= 0;
		m_Id			= CHIPID_IS_COORDINATE;
	}
	
	virtual ~CChipIsCoordinate(){}

	virtual void set_num(int num){m_num = num;}
	virtual void set_operator(int opr){m_operator = opr;}
	
	virtual std::string GetLayoutText(void){
		return
			std::format(
				"{}\n{}{}{}m?",
				m_SelfTgtTypeStr[m_self_tgt.get()],
				m_CoordinateStr[m_param.get()], m_operator_str[m_operator.get()], m_num.get()
			);
	}
	
	virtual void GetBin(CChipBinary& bin){
		CChipCond::GetBin(bin);
		m_self_tgt.GetBin(bin);
		m_param.GetBin(bin);
		m_num.GetBin(bin);
		m_operator.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChipCond::SetBin(bin);
		m_self_tgt.SetBin(bin);
		m_param.SetBin(bin);
		m_num.SetBin(bin);
		m_operator.SetBin(bin);
	}
	
	ScaledInt<1>				m_self_tgt;
	ScaledInt<2>				m_param;
	ScaledInt<7, 3, -168>		m_num;
	ScaledInt<1>				m_operator;
};

class CChipGetCoordinate : public CChip{
public:
	
	CChipGetCoordinate(
		int self_tgt,
		int param,
		int var
	){
		m_self_tgt		= self_tgt;
		m_param			= param;
		m_var			= var;
		m_Id			= CHIPID_GET_COORDINATE;
	}
	
	virtual ~CChipGetCoordinate(){}
	
	virtual std::string GetLayoutText(void){
		return
			std::format(
				"{} = {}{}",
				m_VarNameStr[m_var.get()],
				m_SelfTgtTypeStr[m_self_tgt.get()],
				m_CoordinateStr[m_param.get()]
			);
	}
	
	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_var.GetBin(bin);
		m_self_tgt.GetBin(bin);
		m_param.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		m_var.SetBin(bin);
		m_self_tgt.SetBin(bin);
		m_param.SetBin(bin);
	}
	
	ScaledInt<3>			m_var;
	ScaledInt<1>			m_self_tgt;
	ScaledInt<2>			m_param;
};

//////////////////////////////////////////////////////////////////////////////
// ターゲット状態

class CChipTgtAction : public CChipCond {
public:
	
	enum {
		STOP,
		MOVE,
		TURN,
		JUMP,
		FIRE,
		ACTION,
		STUN,
	};
	
	static inline const char *m_StatusTypeStr[] = {
		"静止", "移動", "旋回", "Jmp", "射撃", "アクション", "被弾"
	};
	
	CChipTgtAction(
		int self_tgt,
		int param
	){
		m_self_tgt	= self_tgt;
		m_param		= param;
		m_Id		= CHIPID_IS_ACTION;
	}
	
	virtual ~CChipTgtAction(){}

	virtual std::string GetLayoutText(void){
		return
			std::format(
				"{}\n{}中?",
				m_SelfTgtTypeStr[m_self_tgt.get()], m_StatusTypeStr[m_param.get()]
			);
	}
	
	virtual void GetBin(CChipBinary& bin){
		CChipCond::GetBin(bin);
		m_self_tgt.GetBin(bin);
		m_param.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChipCond::SetBin(bin);
		m_self_tgt.SetBin(bin);
		m_param.SetBin(bin);
	}

	ScaledInt<1>	m_self_tgt;
	ScaledInt<3>	m_param;
};

static CChipTree is_self_target_status(
	int self_tgt,
	int param
){
	return CCond(new CChipTgtAction(self_tgt, param)).GetCChipTree();
}

#ifndef NO_OKECC_SYNTAX
static CChipTree is_self_stop   (LastLocationArg){LastLocation(); return is_self_target_status(CChip::SELF, CChipTgtAction::STOP);}
static CChipTree is_self_moving (LastLocationArg){LastLocation(); return is_self_target_status(CChip::SELF, CChipTgtAction::MOVE);}
static CChipTree is_self_turning(LastLocationArg){LastLocation(); return is_self_target_status(CChip::SELF, CChipTgtAction::TURN);}
static CChipTree is_self_jumping(LastLocationArg){LastLocation(); return is_self_target_status(CChip::SELF, CChipTgtAction::JUMP);}
static CChipTree is_self_firing (LastLocationArg){LastLocation(); return is_self_target_status(CChip::SELF, CChipTgtAction::FIRE);}
static CChipTree is_self_acting (LastLocationArg){LastLocation(); return is_self_target_status(CChip::SELF, CChipTgtAction::ACTION);}
static CChipTree is_self_stun   (LastLocationArg){LastLocation(); return is_self_target_status(CChip::SELF, CChipTgtAction::STUN);}

static CChipTree is_target_stop   (LastLocationArg){LastLocation(); return is_self_target_status(CChip::TARGET, CChipTgtAction::STOP);}
static CChipTree is_target_moving (LastLocationArg){LastLocation(); return is_self_target_status(CChip::TARGET, CChipTgtAction::MOVE);}
static CChipTree is_target_turning(LastLocationArg){LastLocation(); return is_self_target_status(CChip::TARGET, CChipTgtAction::TURN);}
static CChipTree is_target_jumping(LastLocationArg){LastLocation(); return is_self_target_status(CChip::TARGET, CChipTgtAction::JUMP);}
static CChipTree is_target_firing (LastLocationArg){LastLocation(); return is_self_target_status(CChip::TARGET, CChipTgtAction::FIRE);}
static CChipTree is_target_acting (LastLocationArg){LastLocation(); return is_self_target_status(CChip::TARGET, CChipTgtAction::ACTION);}
static CChipTree is_target_stun   (LastLocationArg){LastLocation(); return is_self_target_status(CChip::TARGET, CChipTgtAction::STUN);}
#endif

//////////////////////////////////////////////////////////////////////////////
// サウンド

class CChipSound : public CChip {
public:
	CChipSound(
		int snd,
		int cnt
	){
		m_Id	= CHIPID_SOUND;
		m_snd	= snd;
		m_cnt	= cnt;
	}

	virtual ~CChipSound(){}
	
	virtual std::string GetLayoutText(void){
		return std::format("♪\n#{}x{}",
			m_snd.get(), m_cnt.get()
		);
	}
	
	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_snd.GetBin(bin);
		m_cnt.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		m_snd.SetBin(bin);
		m_cnt.SetBin(bin);
	}

	ScaledInt<3,1,1> m_snd;
	ScaledInt<3,1,1> m_cnt;
};

static void sound(
	int snd,
	int cnt,
	LastLocationArg
){
	LastLocation();
	g_pCurTree->add(new CChipSound(snd, cnt));
}

//////////////////////////////////////////////////////////////////////////////
// カウンタに自機状態入力

class CChipGetStatus : public CChip {
public:
	
	enum {
		HEAT,
		FUEL,
		DAMAGE,
		AMMO,
	};
	
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
				m_param.get() <= DAMAGE ? m_StatusTypeStr[m_param.get()] :
										  std::format("残弾#{}", m_param.get() - DAMAGE)
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

	ScaledInt<3>			m_var;
	ScaledInt<3>			m_param;
};

//////////////////////////////////////////////////////////////////////////////
// カウンタに自機状態入力

class CChipGetMiscNum : public CChip {
public:
	enum {
		TIME,
		RAND,
		ENEMY_NUM,
		FRIENDLY_NUM,
	};
	
	static inline const char *m_TypeStr[] = {
		"時間", "乱数", "敵機数", "味方機数"
	};
	
	CChipGetMiscNum(int param, int var){
		m_var	= var;
		m_param	= param;
		m_Id	= CHIPID_GET_MISC_NUM;
	}
	
	virtual ~CChipGetMiscNum(){}
	
	virtual std::string GetLayoutText(void){
		return
			std::format(
				"{} = {}",
				m_VarNameStr[m_var.get()],
				m_TypeStr[m_param.get()]
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

	ScaledInt<3>			m_var;
	ScaledInt<2>			m_param;
};

//////////////////////////////////////////////////////////////////////////////
// clamp

class CChipClamp : public CChip {
public:
	CChipClamp(
		int var,
		int max,
		int min
	){
		m_Id	= CHIPID_CLAMP;
		m_var	= var;
		m_min	= min;
		m_max	= max;
	}

	virtual ~CChipClamp(){}
	
	virtual std::string GetLayoutText(void){
		return std::format("クランプ\n{}≦{}≦{}",
			m_min.get(), m_VarNameStr[m_var.get()], m_max.get()
		);
	}
	
	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_var.GetBin(bin);
		m_max.GetBin(bin);
		m_min.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		m_var.SetBin(bin);
		m_max.SetBin(bin);
		m_min.SetBin(bin);
	}

	ScaledInt<3> m_var;
	ScaledInt<8, 1, -127> m_max;
	ScaledInt<8, 1, -127> m_min;
};

static void clamp(
	CChipVar &var,
	int max,
	int min,
	LastLocationArg
){
	LastLocation();
	g_pCurTree->add(new CChipClamp(var.m_var, max, min));
}

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

	ScaledInt<3>			m_var;
	ScaledInt<3, 1, 1>		m_ch;
};

static void ch_send(
	int ch,
	CChipVar& var,
	LastLocationArg
){
	LastLocation();
	g_pCurTree->add(new CChipChSend(ch, var.m_var));
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

	ScaledInt<3>			m_var;
	ScaledInt<3, 1, 1>		m_ch;
};

//////////////////////////////////////////////////////////////////////////////
// val 系

static CChipVal heat  (LastLocationArg){LastLocation(); return CChipVal(CChipVal::HEAT);}
static CChipVal fuel  (LastLocationArg){LastLocation(); return CChipVal(CChipVal::FUEL);}
static CChipVal damage(LastLocationArg){LastLocation(); return CChipVal(CChipVal::DAMAGE);}
static CChipVal time  (LastLocationArg){LastLocation(); return CChipVal(CChipVal::TIME);}
static CChipVal self_x(LastLocationArg){LastLocation(); return CChipVal(CChipVal::SELF_POS_X);}
static CChipVal self_y(LastLocationArg){LastLocation(); return CChipVal(CChipVal::SELF_POS_Y);}
static CChipVal self_z(LastLocationArg){LastLocation(); return CChipVal(CChipVal::SELF_POS_Z);}
static CChipVal target_x(LastLocationArg){LastLocation(); return CChipVal(CChipVal::TGT_POS_X);}
static CChipVal target_y(LastLocationArg){LastLocation(); return CChipVal(CChipVal::TGT_POS_Y);}
static CChipVal target_z(LastLocationArg){LastLocation(); return CChipVal(CChipVal::TGT_POS_Z);}

static CChipVal friendly_num(LastLocationArg){LastLocation(); return CChipVal(CChipVal::FRIENDLY);}
static CChipVal enemy_num(LastLocationArg){LastLocation(); return CChipVal(CChipVal::ENEMY);}
static CChipVal okecc_rand(LastLocationArg){LastLocation(); return CChipVal(CChipVal::RAND);}

static CChipVal ch_receive(int ch, LastLocationArg){LastLocation(); return CChipVal(CChipVal::CH_RECV, ch);}

static CChipVal ammo_num(int weapon, LastLocationArg){
	LastLocation(); return CChipVal(weapon - 1);
}

#ifndef NO_OKECC_SYNTAX
CChipVar& CChipVar::operator=(const CChipVal& val){
	
	CChip *pchip;
	
	switch(val.m_type){
		case CChipVal::HEAT:
		case CChipVal::FUEL:
		case CChipVal::DAMAGE:
			pchip = new CChipGetStatus(val.m_type - CChipVal::HEAT, m_var);
			break;
		
		case CChipVal::TIME:
			pchip = new CChipGetMiscNum(CChipGetMiscNum::TIME, m_var);
			break;
			
		case CChipVal::FRIENDLY:
			pchip = new CChipGetMiscNum(CChipGetMiscNum::FRIENDLY_NUM, m_var);
			break;
			
		case CChipVal::ENEMY:
			pchip = new CChipGetMiscNum(CChipGetMiscNum::ENEMY_NUM, m_var);
			break;
			
		case CChipVal::RAND:
			pchip = new CChipGetMiscNum(CChipGetMiscNum::RAND, m_var);
			break;
		
		case CChipVal::CH_RECV:
			pchip = new CChipChReceive(val.m_param, m_var);
			break;
		
		case CChipVal::SELF_POS_X:
		case CChipVal::SELF_POS_Y:
		case CChipVal::SELF_POS_Z:
		case CChipVal::TGT_POS_X:
		case CChipVal::TGT_POS_Y:
		case CChipVal::TGT_POS_Z:
			pchip = new CChipGetCoordinate(
				val.m_type >= CChipVal::TGT_POS_X ? CChip::TARGET : CChip::SELF,
				(val.m_type - CChipVal::SELF_POS_X) % 3,
				m_var
			);
			break;
		
		default:
			pchip = new CChipGetStatus(CChipGetStatus::AMMO + val.m_type, m_var);
	}
	
	g_pCurTree->add(pchip);
	return *this;
}

CCond CChipVal::GetCChipCond(void) const {
	
	CChip *pchip;
	
	switch(m_type){
		case CChipVal::HEAT:
		case CChipVal::FUEL:
		case CChipVal::DAMAGE:
			pchip = new CChipSelfStatus(m_type - CChipVal::HEAT);
			break;
		
		case CChipVal::TIME:
			pchip = new CChipTime();
			break;
		
		case CChipVal::FRIENDLY:
		case CChipVal::ENEMY:
		case CChipVal::RAND:
			throw OkeccError("Invalid parameter");
			break;
			
		case CChipVal::CH_RECV:
			throw OkeccError("Invalid use of ch_receive()");
			break;
			
		case CChipVal::SELF_POS_X:
		case CChipVal::SELF_POS_Y:
		case CChipVal::SELF_POS_Z:
		case CChipVal::TGT_POS_X:
		case CChipVal::TGT_POS_Y:
		case CChipVal::TGT_POS_Z:
			pchip = new CChipIsCoordinate(
				m_type >= CChipVal::TGT_POS_X ? CChip::TARGET : CChip::SELF,
				(m_type - CChipVal::SELF_POS_X) % 3
			);
			break;
		
		default:
			pchip = new CChipAmmoNum(m_type);
	}
	
	return CCond(pchip);
}

CChipTree CChipVal::operator<=(int num) const {return GetCChipCond() <= num;}
CChipTree CChipVal::operator< (int num) const {return GetCChipCond() <  num;}
CChipTree CChipVal::operator>=(int num) const {return GetCChipCond() >= num;}
CChipTree CChipVal::operator> (int num) const {return GetCChipCond() >  num;}
#endif

//////////////////////////////////////////////////////////////////////////////
// 算術演算

class CChipCalc : public CChip {
public:
	enum {
		ADD, SUB, MUL, DIV, MOD, MOV
	};
	
	static inline const char *m_OprStr[] = {
		"+", "-", "*", "/", "%", ""
	};
	
	CChipCalc(
		int op1,
		int opr,
		int immxvar,
		int op2
	){
		m_op1		= op1;
		m_operator	= opr;
		m_immxvar	= immxvar;
		m_op2		= op2;
		m_Id		= CHIPID_CALC;
	}
	
	virtual ~CChipCalc(){}
	
	virtual std::string GetLayoutText(void){
		return m_immxvar.get() ?
			std::format("{} {}= {}", m_VarNameStr[m_op1.get()], m_OprStr[m_operator.get()], m_op2.get()) :
			std::format("{} {}= {}", m_VarNameStr[m_op1.get()], m_OprStr[m_operator.get()], m_VarNameStr[m_op2.get()]);
	}
	
	virtual void GetBin(CChipBinary& bin){
		CChip::GetBin(bin);
		m_op1.GetBin(bin);
		m_operator.GetBin(bin);
		m_immxvar.GetBin(bin);
		m_op2.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChip::SetBin(bin);
		m_op1.SetBin(bin);
		m_operator.SetBin(bin);
		m_immxvar.SetBin(bin);
		m_op2.SetBin(bin);
	}
	
	ScaledInt<3>	m_op1;
	ScaledInt<3>	m_operator;
	ScaledInt<1>	m_immxvar;
	ScaledInt<8>	m_op2;
};

#ifndef NO_OKECC_SYNTAX
CChipVar& CChipVar::operator+=(const CChipVar& op2){g_pCurTree->add(new CChipCalc(m_var, CChipCalc::ADD, 0, op2.m_var)); return *this;}
CChipVar& CChipVar::operator-=(const CChipVar& op2){g_pCurTree->add(new CChipCalc(m_var, CChipCalc::SUB, 0, op2.m_var)); return *this;}
CChipVar& CChipVar::operator*=(const CChipVar& op2){g_pCurTree->add(new CChipCalc(m_var, CChipCalc::MUL, 0, op2.m_var)); return *this;}
CChipVar& CChipVar::operator/=(const CChipVar& op2){g_pCurTree->add(new CChipCalc(m_var, CChipCalc::DIV, 0, op2.m_var)); return *this;}
CChipVar& CChipVar::operator%=(const CChipVar& op2){g_pCurTree->add(new CChipCalc(m_var, CChipCalc::MOD, 0, op2.m_var)); return *this;}
CChipVar& CChipVar::operator= (const CChipVar& op2){g_pCurTree->add(new CChipCalc(m_var, CChipCalc::MOV, 0, op2.m_var)); return *this;}

CChipVar& CChipVar::operator+=(const int imm){g_pCurTree->add(new CChipCalc(m_var, CChipCalc::ADD, 1, imm)); return *this;}
CChipVar& CChipVar::operator-=(const int imm){g_pCurTree->add(new CChipCalc(m_var, CChipCalc::SUB, 1, imm)); return *this;}
CChipVar& CChipVar::operator*=(const int imm){g_pCurTree->add(new CChipCalc(m_var, CChipCalc::MUL, 1, imm)); return *this;}
CChipVar& CChipVar::operator/=(const int imm){g_pCurTree->add(new CChipCalc(m_var, CChipCalc::DIV, 1, imm)); return *this;}
CChipVar& CChipVar::operator%=(const int imm){g_pCurTree->add(new CChipCalc(m_var, CChipCalc::MOD, 1, imm)); return *this;}
CChipVar& CChipVar::operator= (const int imm){g_pCurTree->add(new CChipCalc(m_var, CChipCalc::MOV, 1, imm)); return *this;}
#endif

//////////////////////////////////////////////////////////////////////////////
// 算術比較

class CChipCmp : public CChipCond {
public:
	CChipCmp(
		int op1,
		int opr,
		int immxvar,
		int op2
	){
		m_op1		= op1;
		m_operator	= opr;
		m_immxvar	= immxvar;
		m_op2		= immxvar ? op2 : op2 - 127;
		m_Id		= CHIPID_CMP;
	}
	
	virtual ~CChipCmp(){}
	
	virtual std::string GetLayoutText(void){
		return m_immxvar.get() ?
			std::format("{}{}{}?", m_VarNameStr[m_op1.get()], m_operator_str[m_operator.get()], m_op2.get()) :
			std::format("{}{}{}?", m_VarNameStr[m_op1.get()], m_operator_str[m_operator.get()], m_VarNameStr[m_op2.get() + 127]);
	}
	
	virtual void GetBin(CChipBinary& bin){
		CChipCond::GetBin(bin);
		m_op1.GetBin(bin);
		m_immxvar.GetBin(bin);
		m_op2.GetBin(bin);
		m_operator.GetBin(bin);
	}

	virtual void SetBin(CChipBinary& bin){
		CChipCond::SetBin(bin);
		m_op1.SetBin(bin);
		m_immxvar.SetBin(bin);
		m_op2.SetBin(bin);
		m_operator.SetBin(bin);
	}
	
	ScaledInt<3>			m_op1;
	ScaledInt<1>			m_immxvar;
	ScaledInt<8, 1, -127>	m_op2;
	ScaledInt<2>			m_operator;
};

#ifndef NO_OKECC_SYNTAX
CChipTree CChipVar::operator>=(const CChipVar& op2) const {return CCond(new CChipCmp(m_var, CChip::OP_GE, 0, op2.m_var)).GetCChipTree();}
CChipTree CChipVar::operator<=(const CChipVar& op2) const {return CCond(new CChipCmp(m_var, CChip::OP_LE, 0, op2.m_var)).GetCChipTree();}
CChipTree CChipVar::operator==(const CChipVar& op2) const {return CCond(new CChipCmp(m_var, CChip::OP_EQ, 0, op2.m_var)).GetCChipTree();}
CChipTree CChipVar::operator!=(const CChipVar& op2) const {return CCond(new CChipCmp(m_var, CChip::OP_NE, 0, op2.m_var)).GetCChipTree();}
CChipTree CChipVar::operator> (const CChipVar& op2) const {return !(*this <= op2);}
CChipTree CChipVar::operator< (const CChipVar& op2) const {return !(*this >= op2);}

CChipTree CChipVar::operator>=(const int imm) const {return CCond(new CChipCmp(m_var, CChip::OP_GE, 1, imm)).GetCChipTree();}
CChipTree CChipVar::operator<=(const int imm) const {return CCond(new CChipCmp(m_var, CChip::OP_LE, 1, imm)).GetCChipTree();}
CChipTree CChipVar::operator==(const int imm) const {return CCond(new CChipCmp(m_var, CChip::OP_EQ, 1, imm)).GetCChipTree();}
CChipTree CChipVar::operator!=(const int imm) const {return CCond(new CChipCmp(m_var, CChip::OP_NE, 1, imm)).GetCChipTree();}
CChipTree CChipVar::operator> (const int imm) const {return CCond(new CChipCmp(m_var, CChip::OP_GE, 1, imm - 1)).GetCChipTree();}
CChipTree CChipVar::operator< (const int imm) const {return CCond(new CChipCmp(m_var, CChip::OP_LE, 1, imm + 1)).GetCChipTree();}
#endif

//////////////////////////////////////////////////////////////////////////////
// if - else - endif

class CBlockInfo;
inline CBlockInfo *g_pCurBlockInfo;

class CBlockInfo {
public:
	static constexpr UINT BLOCK_TOP = 0xFFFFFFFF;
	
	enum {
		BM_NONE,
		BM_IF_TOP,	// top of if block
		BM_IF,		// if / elseif 中
		BM_ELSE,	// else 中
		BM_LOOP,	// loop 中
	};
	
	UINT m_Break;
	
	// stack 構造
	// low <--> high
	// IF:   [BM_IF_TOP] [飛び先] [BM_*] [飛び先] [BM_*] ...
	// Loop: [break先保存] [loop先頭] [BM_LOOP]
	
	std::vector<UINT>	m_BlockStack;
	
	UINT GetMode(void){
		return g_pCurBlockInfo->m_BlockStack.size() < 1 ? BM_NONE :
			g_pCurBlockInfo->m_BlockStack.back();
	}
};

static void if_statement(const CChipTree& cc, LastLocationArg, bool BlockStart = true){
	LastLocation();
	
	if(BlockStart){
		g_pCurBlockInfo->m_BlockStack.push_back(CBlockInfo::BM_IF_TOP);
	}
	
	// CurTree に if 条件のツリーを接続
	g_pCurTree->AddToG(cc.m_start);
	g_pCurTree->m_LastG = cc.m_LastR;

	// false 飛び先を push
	g_pCurBlockInfo->m_BlockStack.push_back(cc.m_LastG);
	
	// mode
	g_pCurBlockInfo->m_BlockStack.push_back(CBlockInfo::BM_IF);
}

static void if_statement(const CCond& chip, LastLocationArg){if_statement(chip.GetCChipTree(), location);}
static void if_statement(const CChipVal&  chip, LastLocationArg){if_statement(chip >= 1, location);}
static void if_statement(const CChipVar&  chip, LastLocationArg){if_statement(chip != 0, location);}

static void else_statement(
	LastLocationArg
){
	LastLocation();

	if(g_pCurBlockInfo->GetMode() != CBlockInfo::BM_IF){
		throw OkeccError("Unexpected else / elseif");
	}
	
	g_pCurBlockInfo->m_BlockStack.pop_back(); // mode
	
	UINT idx = g_pCurTree->m_LastG;	// then 節の最後

	// else 節先頭は，BlockStack に積んでいた false 飛び先
	g_pCurTree->m_LastG = g_pCurBlockInfo->m_BlockStack.back();
	g_pCurBlockInfo->m_BlockStack.pop_back();

	// then 節の最後を block statck に積む
	g_pCurBlockInfo->m_BlockStack.push_back(idx);
	
	// mode
	g_pCurBlockInfo->m_BlockStack.push_back(CBlockInfo::BM_ELSE);
}

static void elseif_statement(CChipTree &&cc, LastLocationArg){
	LastLocation();
	else_statement(location);
	if_statement(cc, location, false);
}

static void elseif_statement(const CCond& chip, LastLocationArg){elseif_statement(chip.GetCChipTree(), location);}
static void elseif_statement(const CChipVal&  chip, LastLocationArg){elseif_statement(chip >= 1, location);}
static void elseif_statement(const CChipVar&  chip, LastLocationArg){elseif_statement(chip != 0, location);}

static void endif_statement(
	LastLocationArg
){
	LastLocation();
	
	if(
		g_pCurBlockInfo->GetMode() != CBlockInfo::BM_IF &&
		g_pCurBlockInfo->GetMode() != CBlockInfo::BM_ELSE
	){
		throw OkeccError("Unexpected endif");
	}
	
	while(
		g_pCurBlockInfo->GetMode() == CBlockInfo::BM_IF ||
		g_pCurBlockInfo->GetMode() == CBlockInfo::BM_ELSE
	){
		if(g_pCurBlockInfo->m_BlockStack.size() < 3){
			throw OkeccError("Internal error: BlockStack broken");
		}
		
		g_pCurBlockInfo->m_BlockStack.pop_back(); // mode
		
		// 合流 GOTO 生成
		UINT merge = g_pCurChipPool->add(new CChipGoto);
		g_pCurTree->AddToG(merge);
		
		// false 条件の飛び先合流
		(*g_pCurChipPool)[g_pCurBlockInfo->m_BlockStack.back()]->m_NextG = merge;
		g_pCurBlockInfo->m_BlockStack.pop_back();
		
		if(g_pCurBlockInfo->GetMode() == CBlockInfo::BM_IF_TOP){
			g_pCurBlockInfo->m_BlockStack.pop_back();
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////
// Loop statement

static void loop_statement(LastLocationArg){
	LastLocation();
	
	UINT LoopTop;
	
	// goto を 2個生成
	g_pCurBlockInfo->m_BlockStack.push_back(g_pCurBlockInfo->m_Break);	// 直前の break 先を保存
	g_pCurBlockInfo->m_BlockStack.push_back(LoopTop = g_pCurChipPool->add(new CChipGoto));	// loop 先頭
	g_pCurBlockInfo->m_Break = g_pCurChipPool->add(new CChipGoto);	// break 先
	
	// Top の goto を接続
	g_pCurTree->AddToG(LoopTop);
	
	// mode
	g_pCurBlockInfo->m_BlockStack.push_back(CBlockInfo::BM_LOOP);
}

static void loopend_statement(LastLocationArg){
	LastLocation();
	
	if(g_pCurBlockInfo->GetMode() != CBlockInfo::BM_LOOP){
		OkeccError("Unexpected endloop");
	}
	g_pCurBlockInfo->m_BlockStack.pop_back(); // mode
	
	UINT LoopTop = g_pCurBlockInfo->m_BlockStack.back();	// loop top
	g_pCurBlockInfo->m_BlockStack.pop_back();
	
	// ループ先頭に接続
	g_pCurTree->AddToG(LoopTop);
	g_pCurTree->m_LastG = g_pCurBlockInfo->m_Break;
	
	// break 先を pop
	g_pCurBlockInfo->m_Break = g_pCurBlockInfo->m_BlockStack.back();
	g_pCurBlockInfo->m_BlockStack.pop_back();
}

static void break_statement(LastLocationArg){
	LastLocation();
	
	if(g_pCurBlockInfo->GetMode() != CBlockInfo::BM_LOOP){
		OkeccError("break not within a loop");
	}
	
	UINT idx = g_pCurTree->m_LastG;
	
	// LastG の place holder
	g_pCurTree->AddToG(g_pCurChipPool->add(new CChipGoto));
	
	// break 先に分岐
	(*g_pCurChipPool)[idx]->m_NextG = g_pCurBlockInfo->m_Break;
}

//////////////////////////////////////////////////////////////////////////////
// end

static void okecc_exit(LastLocationArg){
	LastLocation();
	
	// Goto chip * 2 を置き，1個目を Exit に向ける
	CChip *p;
	g_pCurTree->add((p = new CChipGoto()));
	g_pCurTree->add(new CChipGoto());
	
	p->m_NextG = IDX_EXIT;
}

//////////////////////////////////////////////////////////////////////////////
// C との命名被り回避

#ifndef NO_OKECC_SYNTAX
	#define if(cc)		if_statement(cc);
	#define elseif(cc)	elseif_statement(cc);
	#define else		else_statement();
	#define endif		endif_statement();
	
	#define loop		loop_statement();
	#define endloop		loopend_statement();
	#define break		break_statement()
	
	#define exit		okecc_exit
	#define rand		okecc_rand
#endif
