#define _CRT_SECURE_NO_WARNINGS

#include <algorithm>
#include <cassert>
#include <climits>
#include <cmath>
#include <fcntl.h>
#include <format>
#include <io.h>
#include <iostream>
#include <map>
#include <random>
#include <source_location>
#include <stdexcept>
#include <stdio.h>
#include <string>
#include <vector>
#include <wchar.h>
#include <concepts>

enum {
	OKE_BIPED,
	OKE_QUADRUPED,
	OKE_VEHICLE,
	OKE_HOVER,
	OKE_FLIGHT,
	OKE_ALL,
};

const char *OkeTypeStr[] = {
	"二脚",
	"四脚",
	"車両",
	"ホバー",
	"飛行",
	"全種",
};

enum {
	P_ALL,
	P_BULLET,
	P_MISSILE,
	P_BEAM,
	P_ROCKET,
	P_MINE,
	P_FMINE,
	P_HI_V
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

template<UINT _width, UINT _step = 1, int _offset = 0>
class ScaledInt {
public:
	static constexpr UINT width	= _width;
	static constexpr int offset	= _offset;
	static constexpr UINT step	= _step;
	static constexpr int min	= offset;
	static constexpr int max	= offset + (int)(((1U << width) - 1) * step);

	void set(int val){
		if(val < min || max < val){
			throw OkeccError(std::format("Value {} is out of range [{} - {}]", val, min, max));
		}

		m_value = (UINT)((double)(val - offset) / step + 0.5) & ((1 << width) - 1);
	};

	ScaledInt& operator=(int i){
		set(i);
		return *this;
	}

	int get(void){
		return m_value * step + offset;
	}

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

	ScaledInt<6>	m_Id;
	ScaledInt<3>	m_RawG;
	ScaledInt<3>	m_RawR;

	UINT			m_NextG = IDX_NONE;
	UINT			m_NextR = IDX_NONE;
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

CChipPool	*g_pCurChipPool;

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

	bool ValidStart(void) const {return m_start < IDX_EXIT;}
	bool ValidG    (void) const {return m_LastG < IDX_EXIT;}
	bool ValidR    (void) const {return m_LastR < IDX_EXIT;}

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

CChipTree *g_pCurTree;

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

class CChipCond {
public:
	
	CChipCond(CChip *pchip) : m_pchip(pchip){}
	
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
	
	CChipCond GetCChipCond(void) const;
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

CChipVar A(0);
CChipVar B(1);
CChipVar C(2);
CChipVar D(3);
CChipVar E(4);
CChipVar F(5);

//////////////////////////////////////////////////////////////////////////////
// NOP

static constexpr UINT NOP_AE = 62;

class CChipNop : public CChip {
public:
	CChipNop(UINT param){
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
		LEFT,
		RIGHT,
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
			m_param.get() == LEFT	? "左移動" :
			m_param.get() == RIGHT	? "右移動" :
									  "停止";
	}
	
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
		LEFT,
		RIGHT,
	};

	CChipTurn(UINT param){
		m_Id	= CHIPID_TURN;
		m_param	= param;
	}

	virtual ~CChipTurn(){}

	virtual std::string GetLayoutText(void){
		return m_param.get() == LEFT	? "左旋回" : "右旋回";
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
		LEFT,
		RIGHT,
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
	
	ScaledInt<2> m_param;
};

static void put_action_chip(UINT param){
	g_pCurTree->add(new CChipAction(param));
}

static void fight  (LastLocationArg){LastLocation(); put_action_chip(CChipAction::FIGHT);}
static void crouch (LastLocationArg){LastLocation(); put_action_chip(CChipAction::CROUCH);}
static void guard  (LastLocationArg){LastLocation(); put_action_chip(CChipAction::GUARD);}
static void action1(LastLocationArg){LastLocation(); put_action_chip(CChipAction::ACTION1);}
static void action2(LastLocationArg){LastLocation(); put_action_chip(CChipAction::ACTION2);}
static void action3(LastLocationArg){LastLocation(); put_action_chip(CChipAction::ACTION3);}

//////////////////////////////////////////////////////////////////////////////
// alt

class CChipAlt : public CChip {
public:
	enum {
		HIGH,
		MID,
		LOW,
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
	
	ScaledInt<5, 16, -240>	m_angleCenter;
	ScaledInt<4, 32, 32>	m_angleRange;
	ScaledInt<4, 20, 20>	m_distance;
	ScaledInt<3>			m_type;
	ScaledInt<3>			m_weapon;
	ScaledInt<3, 1, 1>		m_cnt;
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
		m_elevation	= elevation;
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
				m_direction.get(), m_elevation.get()
			);
	}
	
	ScaledInt<5, 16, -240>	m_direction;
	ScaledInt<4, 16, -112>	m_elevation;
	ScaledInt<3>			m_weapon;
	ScaledInt<3, 1, 1>		m_cnt;
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
	
	ScaledInt<3>			m_weapon;
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
	
	ScaledInt<3>		m_direction;
	ScaledInt<3>		m_elevation;
	ScaledInt<3>		m_weapon;
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
	
	ScaledInt<2, 1, 1> m_param;
};

static void option(UINT param, LastLocationArg){
	LastLocation();
	g_pCurTree->add(new CChipOption(param));
}

//////////////////////////////////////////////////////////////////////////////
// 残弾

class CChipAmmoNum : public CChip {
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
	
	ScaledInt<3>		m_weapon;
	ScaledInt<7, 1, 1>	m_num;
	ScaledInt<1>		m_operator;
};

static CChipCond option_num(
	int weapon,
	LastLocationArg
){
	LastLocation();
	return CChipCond(new CChipAmmoNum(weapon + 4));
}

//////////////////////////////////////////////////////////////////////////////
// 近くの OKE を探索

class CChipOkeNum : public CChip{
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
	
	ScaledInt<5, 16, -240>	m_angleCenter;
	ScaledInt<4, 32, 32>	m_angleRange;
	ScaledInt<4, 20, 20>	m_distance;
	ScaledInt<1>		 	m_enemy;
	ScaledInt<3>			m_type;
	ScaledInt<2, 1, 1>		m_num;
	ScaledInt<1>			m_operator;
};

static CChipCond oke_num(
	int angleCenter,
	int angleRange,
	int distance,
	int type,
	int enemy
){
	return CChipCond(new CChipOkeNum(angleCenter, angleRange, distance, type, enemy));
}

static CChipCond enemy_num(
	int angleCenter,
	int angleRange,
	int distance,
	int type,
	LastLocationArg
){
	LastLocation();
	return oke_num(angleCenter, angleRange, distance, type, CChip::ENEMY);
}

static CChipCond friendly_num(
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

class CChipBarrier : public CChip {
public:

	CChipBarrier(
		int angleCenter,
		int angleRange,
		int distance
	){
		m_angleCenter	= angleCenter;
		m_angleRange	= angleRange;
		m_distance		= distance;
		m_operator		= OP_GE;
		m_num			= 3;
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
	
	ScaledInt<5, 16, -240>	m_angleCenter;
	ScaledInt<4, 32, 32>	m_angleRange;
	ScaledInt<4, 10, 10>	m_distance;
	ScaledInt<3, 3, 3>		m_num;
	ScaledInt<1>			m_operator;
};

static CChipCond barrier_height(
	int angleCenter,
	int angleRange,
	int distance,
	LastLocationArg
){
	LastLocation();
	return CChipCond(new CChipBarrier(angleCenter, angleRange, distance));
}

//////////////////////////////////////////////////////////////////////////////
// 近くの危険物を探索

class CChipProjectileNum : public CChip {
public:
	static inline const char *m_ProjectileTypeStr[] = {
		"危険物", "弾丸", "ミサイル", "ビーム", "ロケット", "地雷", "機雷", "高速"
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
	
	ScaledInt<5, 16, -240>	m_angleCenter;
	ScaledInt<4, 32, 32>	m_angleRange;
	ScaledInt<4, 10, 10>	m_distance;
	ScaledInt<3>			m_type;
	ScaledInt<3, 1, 1>		m_num;
	ScaledInt<1>			m_operator;
};

static CChipCond projectile_num(
	int angleCenter,
	int angleRange,
	int distance,
	int type,
	LastLocationArg
){
	LastLocation();
	return CChipCond(new CChipProjectileNum(angleCenter, angleRange, distance, type));
}

//////////////////////////////////////////////////////////////////////////////
// マップポイント

class CChipMapPoint : public CChip {
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
	
	ScaledInt<5, 16, -240>	m_angleCenter;
	ScaledInt<4, 32, 32>	m_angleRange;
	ScaledInt<4, 20, 20>	m_distance;
	ScaledInt<3, 1, 1>		m_x;
	ScaledInt<3>			m_y;
};

static CChipCond is_mappoint(
	int angleCenter,
	int angleRange,
	int distance,
	int map_x,
	int map_y,
	LastLocationArg
){
	LastLocation();
	return CChipCond(new CChipMapPoint(angleCenter, angleRange, distance, map_x, map_y));
}

//////////////////////////////////////////////////////////////////////////////
// 自機の状態確認

class CChipSelfStatus : public CChip {
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
	
	ScaledInt<2,1,1> m_no;
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

class CChipIsRand : public CChip {
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
	
	ScaledInt<6, 1, 1>		m_num1;
	ScaledInt<6, 1, 1>		m_num2;
};

static CChipTree is_rand(
	int num1,
	int num2,
	LastLocationArg
){
	LastLocation();
	return CChipCond(new CChipIsRand(num1, num2)).GetCChipTree();
}

//////////////////////////////////////////////////////////////////////////////
// 時間

class CChipTime : public CChip {
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
	
	ScaledInt<5, 16, -240>	m_angleCenter;
	ScaledInt<4, 32, 32>	m_angleRange;
	ScaledInt<4, 20, 20>	m_distance;
	ScaledInt<1>		 	m_enemy;
	ScaledInt<3>			m_type;
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

class CChipTgtDistance : public CChip {
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
	
	ScaledInt<6, 20>		m_num;
	ScaledInt<1>			m_operator;
};

static CChipCond target_distance(
	LastLocationArg
){
	LastLocation();
	return CChipCond(new CChipTgtDistance());
}

//////////////////////////////////////////////////////////////////////////////
// ターゲット方向

class CChipTgtDirection : public CChip {
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
	
	ScaledInt<5, 16, -240>	m_angleCenter;
	ScaledInt<4, 32, 32>	m_angleRange;
};

static CChipTree is_target_direction(
	int angleCenter,
	int angleRange,
	LastLocationArg
){
	LastLocation();
	return CChipCond(new CChipTgtDirection(angleCenter, angleRange)).GetCChipTree();
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

class CChipIsCoordinate : public CChip{
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
	
	ScaledInt<1>		 	m_self_tgt;
	ScaledInt<2>			m_param;
	ScaledInt<7, 3, -168>	m_num;
	ScaledInt<1>			m_operator;
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
	
	ScaledInt<3>			m_var;
	ScaledInt<1>			m_self_tgt;
	ScaledInt<2>			m_param;
};

//////////////////////////////////////////////////////////////////////////////
// ターゲット状態

class CChipTgtAction : public CChip {
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
	
	ScaledInt<1>	m_self_tgt;
	ScaledInt<3>	m_param;
};

static CChipTree is_self_target_status(
	int self_tgt,
	int param
){
	return CChipCond(new CChipTgtAction(self_tgt, param)).GetCChipTree();
}

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
		FRIENDLY_NUM,
		ENEMY_NUM,
	};
	
	static inline const char *m_TypeStr[] = {
		"時間", "乱数", "味方機数", "敵機数"
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
	
	ScaledInt<3>			m_var;
	ScaledInt<2>			m_param;
};

//////////////////////////////////////////////////////////////////////////////
// clamp

class CChipClamp : public CChip {
public:
	CChipClamp(
		int var,
		int min,
		int max
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
	
	ScaledInt<3> m_var;
	ScaledInt<8, 1, -127> m_min;
	ScaledInt<8, 1, -127> m_max;
};

static void clamp(
	CChipVar &var,
	int min,
	int max,
	LastLocationArg
){
	LastLocation();
	g_pCurTree->add(new CChipClamp(var.m_var, min, max));
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
	
	ScaledInt<3>			m_var;
	ScaledInt<3, 1, 1>		m_ch;
};

//////////////////////////////////////////////////////////////////////////////
// val 系

CChipVal heat  (LastLocationArg){LastLocation(); return CChipVal(CChipVal::HEAT);}
CChipVal fuel  (LastLocationArg){LastLocation(); return CChipVal(CChipVal::FUEL);}
CChipVal damage(LastLocationArg){LastLocation(); return CChipVal(CChipVal::DAMAGE);}
CChipVal time  (LastLocationArg){LastLocation(); return CChipVal(CChipVal::TIME);}
CChipVal self_x(LastLocationArg){LastLocation(); return CChipVal(CChipVal::SELF_POS_X);}
CChipVal self_y(LastLocationArg){LastLocation(); return CChipVal(CChipVal::SELF_POS_Y);}
CChipVal self_z(LastLocationArg){LastLocation(); return CChipVal(CChipVal::SELF_POS_Z);}
CChipVal target_x(LastLocationArg){LastLocation(); return CChipVal(CChipVal::TGT_POS_X);}
CChipVal target_y(LastLocationArg){LastLocation(); return CChipVal(CChipVal::TGT_POS_Y);}
CChipVal target_z(LastLocationArg){LastLocation(); return CChipVal(CChipVal::TGT_POS_Z);}

CChipVal friendly_num(LastLocationArg){LastLocation(); return CChipVal(CChipVal::FRIENDLY);}
CChipVal enemy_num(LastLocationArg){LastLocation(); return CChipVal(CChipVal::ENEMY);}
CChipVal okecc_rand(LastLocationArg){LastLocation(); return CChipVal(CChipVal::RAND);}

CChipVal ch_receive(int ch, LastLocationArg){LastLocation(); return CChipVal(CChipVal::CH_RECV, ch);}

CChipVal ammo_num(int weapon, LastLocationArg){
	LastLocation(); return CChipVal(weapon - 1);
}

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

CChipCond CChipVal::GetCChipCond(void) const {
	
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
	
	return CChipCond(pchip);
}

CChipTree CChipVal::operator<=(int num) const {return GetCChipCond() <= num;}
CChipTree CChipVal::operator< (int num) const {return GetCChipCond() <  num;}
CChipTree CChipVal::operator>=(int num) const {return GetCChipCond() >= num;}
CChipTree CChipVal::operator> (int num) const {return GetCChipCond() >  num;}

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
	
	ScaledInt<3>	m_op1;
	ScaledInt<3>	m_operator;
	ScaledInt<1>	m_immxvar;
	ScaledInt<8>	m_op2;
};

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

//////////////////////////////////////////////////////////////////////////////
// 算術比較

class CChipCmp : public CChip {
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
		m_op2		= op2;
		m_Id		= CHIPID_CMP;
	}
	
	virtual ~CChipCmp(){}
	
	virtual std::string GetLayoutText(void){
		return m_immxvar.get() ?
			std::format("{}{}{}?", m_VarNameStr[m_op1.get()], m_operator_str[m_operator.get()], m_op2.get()) :
			std::format("{}{}{}?", m_VarNameStr[m_op1.get()], m_operator_str[m_operator.get()], m_VarNameStr[m_op2.get()]);
	}
	
	ScaledInt<3>	m_op1;
	ScaledInt<1>	m_immxvar;
	ScaledInt<8>	m_op2;
	ScaledInt<3>	m_operator;
};

CChipTree CChipVar::operator>=(const CChipVar& op2) const {return CChipCond(new CChipCmp(m_var, CChip::OP_GE, 0, op2.m_var)).GetCChipTree();}
CChipTree CChipVar::operator<=(const CChipVar& op2) const {return CChipCond(new CChipCmp(m_var, CChip::OP_LE, 0, op2.m_var)).GetCChipTree();}
CChipTree CChipVar::operator==(const CChipVar& op2) const {return CChipCond(new CChipCmp(m_var, CChip::OP_EQ, 0, op2.m_var)).GetCChipTree();}
CChipTree CChipVar::operator!=(const CChipVar& op2) const {return CChipCond(new CChipCmp(m_var, CChip::OP_NE, 0, op2.m_var)).GetCChipTree();}
CChipTree CChipVar::operator> (const CChipVar& op2) const {return !(*this <= op2);}
CChipTree CChipVar::operator< (const CChipVar& op2) const {return !(*this >= op2);}

CChipTree CChipVar::operator>=(const int imm) const {return CChipCond(new CChipCmp(m_var, CChip::OP_GE, 1, imm)).GetCChipTree();}
CChipTree CChipVar::operator<=(const int imm) const {return CChipCond(new CChipCmp(m_var, CChip::OP_LE, 1, imm)).GetCChipTree();}
CChipTree CChipVar::operator==(const int imm) const {return CChipCond(new CChipCmp(m_var, CChip::OP_EQ, 1, imm)).GetCChipTree();}
CChipTree CChipVar::operator!=(const int imm) const {return CChipCond(new CChipCmp(m_var, CChip::OP_NE, 1, imm)).GetCChipTree();}
CChipTree CChipVar::operator> (const int imm) const {return CChipCond(new CChipCmp(m_var, CChip::OP_GE, 1, imm - 1)).GetCChipTree();}
CChipTree CChipVar::operator< (const int imm) const {return CChipCond(new CChipCmp(m_var, CChip::OP_LE, 1, imm + 1)).GetCChipTree();}

//////////////////////////////////////////////////////////////////////////////
// if - else - endif

static constexpr UINT BLOCK_TOP = 0xFFFFFFFF;
std::vector<UINT>	g_IfBlockStack;

static bool BlockStackUdf(void){
	return g_IfBlockStack.size() < 1 || g_IfBlockStack.back() == BLOCK_TOP;
}

void if_statement(const CChipTree& cc, LastLocationArg, bool BlockStart = true){
	LastLocation();
	
	// block top mark
	if(BlockStart) g_IfBlockStack.push_back(BLOCK_TOP);
	
	// CurTree に if 条件のツリーを接続
	g_pCurTree->AddToG(cc.m_start);
	g_pCurTree->m_LastG = cc.m_LastR;

	// false 飛び先を push
	g_IfBlockStack.push_back(cc.m_LastG);
}

void if_statement(const CChipCond& chip, LastLocationArg){if_statement(chip.GetCChipTree(), location);}
void if_statement(const CChipVal&  chip, LastLocationArg){if_statement(chip >= 1, location);}
void if_statement(const CChipVar&  chip, LastLocationArg){if_statement(chip != 0, location);}

void else_statement(
	LastLocationArg
){
	LastLocation();

	if(BlockStackUdf()){
		throw OkeccError("Unexpected else");
	}

	UINT idx = g_pCurTree->m_LastG;	// then 節の最後

	// else 節先頭は，BlockStack に積んでいた false 飛び先
	g_pCurTree->m_LastG = g_IfBlockStack.back(); g_IfBlockStack.pop_back();

	// then 節の最後を block statck に積む
	g_IfBlockStack.push_back(idx);
}

void elseif_statement(CChipTree &&cc, LastLocationArg){
	LastLocation();
	else_statement(location);
	if_statement(cc, location, false);
}

void endif_statement(
	LastLocationArg
){
	LastLocation();
	
	if(BlockStackUdf()){
		throw OkeccError("Unexpected endif");
	}
	
	while(1){
		UINT idx = g_IfBlockStack.back(); g_IfBlockStack.pop_back();
		if(idx == BLOCK_TOP) break;
		
		// 合流 GOTO 生成
		UINT merge = g_pCurChipPool->add(new CChipGoto);
		g_pCurTree->AddToG(merge);
		
		// false 条件の飛び先合流
		(*g_pCurChipPool)[idx]->m_NextG = merge;
	}
}

//////////////////////////////////////////////////////////////////////////////
// Loop statement

std::vector<UINT>	g_LoopBlockStack;

void loop_statement(LastLocationArg){
	LastLocation();
	
	UINT LoopTop;
	
	// goto を 2個生成
	g_LoopBlockStack.push_back(LoopTop = g_pCurChipPool->add(new CChipGoto));	// loop 先頭
	g_LoopBlockStack.push_back(g_pCurChipPool->add(new CChipGoto));				// break 先
	
	// Top の goto を接続
	g_pCurTree->AddToG(LoopTop);
}

void loopend_statement(LastLocationArg){
	LastLocation();
	
	if(g_LoopBlockStack.size() == 0){
		OkeccError("Unexpected endloop");
	}
	
	UINT LoopExit = g_LoopBlockStack.back();	// break 先
	g_LoopBlockStack.pop_back();
	
	UINT LoopTop = g_LoopBlockStack.back();		// loop top
	g_LoopBlockStack.pop_back();
	
	// ループ先頭に接続
	g_pCurTree->AddToG(LoopTop);
	
	g_pCurTree->m_LastG = LoopExit;
}

void break_statement(LastLocationArg){
	LastLocation();
	
	if(g_LoopBlockStack.size() < 0){
		OkeccError("break found out of loop");
	}
	
	UINT idx = g_pCurTree->m_LastG;
	
	// LastG の place holder
	g_pCurTree->AddToG(g_pCurChipPool->add(new CChipGoto));
	
	// break 先に分岐
	(*g_pCurChipPool)[idx]->m_NextG = g_LoopBlockStack[g_LoopBlockStack.size() - 1];
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
// 焼きなまし法

struct Pos {
	int x, y;
	bool operator<(const Pos& other) const {
		if (x != other.x) return x < other.x;
		return y < other.y;
	}
};

struct PathNode {
	UINT id;
	int dist_to_exit; // EXITまでの最大距離
};

// Y座標を指定し、それ以降のチップを一括シフトする
void shift_y_range(std::vector<Pos>& state, int threshold_y, int delta) {
	for (auto& p : state) {
		if (delta > 0 && p.y >= threshold_y) p.y += delta;
		else if (delta < 0 && p.y <= threshold_y) p.y += delta;
	}
}

// 座標が未決定、または論理的に削除されたことを示す定数
static constexpr int POS_INVALID = -100;

class CarnageSA {
	CChipPool& pool;
	int grid_width;
	int grid_height;
	std::vector<Pos> state;
	std::mt19937 gen;
	const char *m_svg_file;

	// 8方向のベクトル定義 (N, NE, E, SE, S, SW, W, NW)
	inline static const int dx[] = { 0,  1,  1,  1,  0, -1, -1, -1};
	inline static const int dy[] = {-1, -1,  0,  1,  1,  1,  0, -1};
	
public:
	CarnageSA(CChipPool& p, const char *svg_file = nullptr)
		: pool(p), grid_width(p.m_width), grid_height(p.m_height), gen(42/*std::random_device{}()*/), m_svg_file(svg_file){
		initialize();
	}
	
	// 座標の正規化：最小値を (0, 0) に合わせる
	void normalize_coordinates() {
		int min_x = INT_MAX, min_y = INT_MAX;
		bool has_data = false;
	
		for (const auto& p : state) {
			if (p.x != INT_MAX) {
				min_x = std::min(min_x, p.x);
				min_y = std::min(min_y, p.y);
				has_data = true;
			}
		}
	
		if (!has_data) return;
	
		for (auto& p : state) {
			if (p.x != INT_MAX) {
				p.x -= min_x;
				p.y -= min_y;
			}
		}
	}
	
	void initialize(void);
	std::vector<int> calculate_max_distances(void);
	UINT find_join_node(UINT start_idx, const std::vector<int>& dists);
	void run();
	double calculate_energy();
	bool has_any_intersection();
	bool is_invalid_layout(const std::vector<Pos>& current_state);
	void finalize();
	void OutputSvg(const char* filename, const std::vector<Pos>& state_disp);
	bool cleanup_gotos();
	
	const std::vector<Pos>& get_result() const { return state; }
};

std::vector<int> CarnageSA::calculate_max_distances() {
	const int num_chips = (int)pool.size();
	// -1 は「まだ EXIT までの距離が不明」または「到達不能」を意味する
	std::vector<int> dists(num_chips, -1);

	// 1. 初期状態：EXIT に直接つながっているチップの距離を 1 とする
	for (int i = 0; i < num_chips; ++i) {
		if (pool[i]->m_NextG == IDX_EXIT || 
		   (pool[i]->ValidR() && pool[i]->m_NextR == IDX_EXIT)) {
			dists[i] = 1;
		}
	}

	// 2. 最大 chip 数分だけ反復して距離を更新する（ベルマン・フォード法）
	// 最長パスを求めるため、dists[next] + 1 が現在の dists[i] より大きければ更新
	for (int iter = 0; iter < num_chips; ++iter) {
		bool changed = false;
		for (int i = 0; i < num_chips; ++i) {
			auto update = [&](UINT next) {
				if (next < (UINT)num_chips && dists[next] != -1) {
					if (dists[i] < dists[next] + 1) {
						dists[i] = dists[next] + 1;
						changed = true;
					}
				}
			};

			update(pool[i]->m_NextG);
			if (pool[i]->ValidR()) {
				update(pool[i]->m_NextR);
			}
		}
		// 更新がなくなれば、すべての最短ではない「最長パス」が確定したことになる
		if (!changed) break;
	}

	return dists;
}

void CarnageSA::initialize() {
	// 0. 準備
	const int original_num_chips = (int)pool.m_list.size();
	state.assign(original_num_chips, {INT_MAX, INT_MAX});
	
	// 垂直方向の接続線が存在する X 座標を記録（衝突回避用）
	std::vector<int> vertical_lines_x;
	
	// 乱数分布の定義 (0か1)
	std::uniform_int_distribution<int> coin_flip(0, 1);

	// --- 1. ヘルパー関数 ---

	// EXIT までの最長ステップ数を計算
	auto calculate_max_distances = [&]() {
		std::vector<int> dists(original_num_chips, -1);
		for (int i = 0; i < original_num_chips; ++i) {
			if (pool.m_list[i]->m_NextG == IDX_EXIT || 
			   (pool.m_list[i]->ValidR() && pool.m_list[i]->m_NextR == IDX_EXIT)) {
				dists[i] = 1;
			}
		}
		for (int iter = 0; iter < original_num_chips; ++iter) {
			bool changed = false;
			for (int i = 0; i < original_num_chips; ++i) {
				auto update = [&](UINT from, UINT next) {
					if (next < (UINT)original_num_chips && dists[next] != -1) {
						if (dists[from] < dists[next] + 1) {
							dists[from] = dists[next] + 1;
							changed = true;
						}
					}
				};
				update(i, pool.m_list[i]->m_NextG);
				if (pool.m_list[i]->ValidR()) update(i, pool.m_list[i]->m_NextR);
			}
			if (!changed) break;
		}
		return dists;
	};
	std::vector<int> dists = calculate_max_distances();

	auto get_d = [&](UINT idx) -> int {
		if (idx == IDX_EXIT) return 0;
		if (idx >= (UINT)dists.size() || dists[idx] == -1) return -100;
		return dists[idx];
	};

	auto modify_next = [&](UINT from, UINT old_to, UINT new_to) {
		if (from >= (UINT)pool.m_list.size()) return;
		CChip* f = pool.m_list[from];
		if (f->m_NextG == old_to) f->m_NextG = new_to;
		else if (f->ValidR() && f->m_NextR == old_to) f->m_NextR = new_to;
	};

	auto add_goto_chip = [&](int x, int y) -> UINT {
		UINT new_id = (UINT)pool.m_list.size();
		pool.m_list.push_back(new CChipGoto()); 
		state.push_back({x, y});
		return new_id;
	};

	auto find_join_node = [&](UINT start_idx) -> UINT {
		UINT curr = start_idx;
		while (curr != IDX_EXIT && curr != IDX_NONE) {
			if (curr < (UINT)state.size() && state[curr].x != INT_MAX) return curr;
			bool has_r = (curr < (UINT)original_num_chips) ? pool.m_list[curr]->ValidR() : false;
			int dg = (curr < (UINT)original_num_chips) ? get_d(pool.m_list[curr]->m_NextG) : -100;
			int dr = (curr < (UINT)original_num_chips && has_r) ? get_d(pool.m_list[curr]->m_NextR) : -100;
			curr = (dg >= dr) ? pool.m_list[curr]->m_NextG : pool.m_list[curr]->m_NextR;
		}
		return IDX_NONE;
	};

	auto count_path_chips = [&](UINT start, UINT goal) -> int {
		if (start == goal) return 0;
		int count = 0;
		UINT curr = start;
		while (curr != goal && curr != IDX_EXIT && curr != IDX_NONE && count < 256) {
			count++;
			bool has_r = (curr < (UINT)original_num_chips) ? pool.m_list[curr]->ValidR() : false;
			int dg = (curr < (UINT)original_num_chips) ? get_d(pool.m_list[curr]->m_NextG) : -100;
			int dr = (curr < (UINT)original_num_chips && has_r) ? get_d(pool.m_list[curr]->m_NextR) : -100;
			curr = (dg >= dr) ? pool.m_list[curr]->m_NextG : pool.m_list[curr]->m_NextR;
		}
		return count;
	};

	// --- 2. 再帰配置関数 ---
	struct BranchTask { UINT parent; UINT target; };
	std::vector<BranchTask> pending_tasks;
	
	auto place_path = [&](UINT start_idx, int start_x, int start_y) -> void {
		
		UINT curr = start_idx;
		int cur_x = start_x;

		// STEP 1: 現在の行に沿って主軸を配置
		while (curr != IDX_EXIT && curr != IDX_NONE) {
			// 合流判定: 既に座標があるチップに到達したらこのパスの配置は終了
			if (curr < (UINT)state.size() && state[curr].x != INT_MAX) {
				// 必要ならここで GoTo による水平接続の延長を行う
				vertical_lines_x.push_back(cur_x - 1);
				break;
			}

			if (curr >= (UINT)state.size()) state.resize(curr + 1, {INT_MAX, INT_MAX});
			state[curr] = {cur_x, start_y};

			UINT next_node = IDX_NONE;
			if (curr < (UINT)original_num_chips) {
				CChip* chip = pool.m_list[curr];
				if (chip->ValidR()) {
					int dg = get_d(chip->m_NextG);
					int dr = get_d(chip->m_NextR);
					UINT other;
					if (dg >= dr) {
						next_node = chip->m_NextG;
						other = chip->m_NextR;
					} else {
						next_node = chip->m_NextR;
						other = chip->m_NextG;
					}
					if (other != IDX_EXIT && other != IDX_NONE) {
						// 未処理の分岐として保存
						pending_tasks.push_back({curr, other});
					}
				} else {
					next_node = chip->m_NextG;
				}
			} else {
				next_node = pool.m_list[curr]->m_NextG;
			}
			curr = next_node;
			cur_x++;
		}
	};
	
	// 3. 実行
	place_path(0, 0, 0);
	
	// STEP 2: 保存した分岐タスクの処理
	for(int i = 0; i < pending_tasks.size(); ++i){
		auto& task = pending_tasks[i];
		
		// 他のパス（異なる y 座標）ですでに配置済みなら合流とみなしてスキップ
		if (state[task.target].x != INT_MAX && state[task.target].y != state[task.parent].y) {
			continue; 
		}

		UINT join_node = find_join_node(task.target);

		int bx = state[task.parent].x + 1;
		
		// move_up の決定: 垂直衝突があれば上へ、なければランダム
		bool move_up;
		if (std::find(vertical_lines_x.begin(), vertical_lines_x.end(), bx) != vertical_lines_x.end()) {
			move_up = true;
		} else {
			move_up = (coin_flip(this->gen) == 0); // メンバ変数の gen を明示的に使用
		}

		int target_y = move_up ? state[task.parent].y - 1 : state[task.parent].y + 1;

		// 他のチップを押し出す
		shift_y_range(state, target_y, move_up ? -1 : 1);

		if (join_node != IDX_NONE) {	// IDX_NONE は，そのまま exit して合流点がないことを意味する
			int target_bx = state[join_node].x - 1;
			int path_len = count_path_chips(task.target, join_node);
			int other_path_len = target_bx - bx + 1;

			if (path_len == 0) {
				if(other_path_len < 2) {
					// goto を 1個で迂回
					UINT g = add_goto_chip(bx, target_y);
					modify_next(task.parent, task.target, g);
					pool.m_list[g]->m_NextG = join_node;
				}else{
					// 即時合流(0 chip): コの字迂回 GoTo 生成
					UINT g1 = add_goto_chip(bx, target_y);
					UINT g2 = add_goto_chip(std::max(bx, target_bx), target_y);
					modify_next(task.parent, task.target, g1);
					pool.m_list[g1]->m_NextG = g2;
					pool.m_list[g2]->m_NextG = join_node;
				}
				continue;
			}else if(other_path_len > path_len) {
				/*
				// 1 chip: ターゲットチップ自体の配置後に GoTo で合流を補助
				UINT g1 = add_goto_chip(std::max(bx + 1, target_bx), target_y);
				pool.m_list[g1]->m_NextG = join_node;
				modify_next(task.target, join_node, g1);
				*/
			}
		}

		// 枝パスの再帰配置
		place_path(task.target, bx, target_y);
	}

	normalize_coordinates();
	
	if(m_svg_file) OutputSvg(m_svg_file, state);
}

double CarnageSA::calculate_energy() {
	double energy = 0.0;
	const int n = (int)state.size();
	
	// グリッドサイズを取得
	const int width = pool.m_width;
	const int height = pool.m_height;

	const double PENALTY_BASE_OUT = 20000.0;
	const double PENALTY_CONSTRAINT = 15000.0; // 制約違反（START/EXIT）
	const double PENALTY_OVERLAP = 10000.0;
	const double COST_DISTANCE = 1.0;

	for (int i = 0; i < n; ++i) {
		if (state[i].x == POS_INVALID) continue;

		// --- 1. 枠外判定 & 基本配置制約 ---
		int x = state[i].x;
		int y = state[i].y;

		// 通常の枠外ペナルティ
		if (x < 0 || x >= width || y < 0 || y >= height) {
			int dx = (x < 0) ? -x : (x >= width ? x - (width - 1) : 0);
			int dy = (y < 0) ? -y : (y >= height ? y - (height - 1) : 0);
			energy += PENALTY_BASE_OUT + (double)(dx * dx + dy * dy) * 5000.0;
		}

		// --- 2. STARTチップの制約 (Y=0でなければならない) ---
		if ((UINT)i == pool.m_start) {
			if (y != 0) {
				energy += PENALTY_CONSTRAINT + (double)(y * y) * 2000.0;
			}
		}

		// --- 3. 配線・EXIT制約の評価 ---
		auto evaluate_conn = [&](UINT next_idx) {
			// EXITへの接続チェック
			if (next_idx == IDX_EXIT) {
				// EXITに繋がるチップは外枠(x=0, x=w-1, y=0, y=h-1)に接する必要がある
				bool on_edge = (x == 0 || x == width - 1 || y == 0 || y == height - 1);
				if (!on_edge) {
					// 外枠までの最短距離をペナルティに加算
					int dist_x = std::min(x, (width - 1) - x);
					int dist_y = std::min(y, (height - 1) - y);
					int edge_dist = std::min(dist_x, dist_y);
					energy += PENALTY_CONSTRAINT + (double)(edge_dist * edge_dist) * 2000.0;
				}
				return;
			}

			if (next_idx == IDX_NONE || next_idx >= (UINT)n) return;
			if (state[next_idx].x == POS_INVALID) return;

			// 通常の配線距離コスト
			int x2 = state[next_idx].x;
			int y2 = state[next_idx].y;
			energy += (std::max(std::abs(x - x2), std::abs(y - y2)) - 1) * COST_DISTANCE;
		};

		evaluate_conn(pool.m_list[i]->m_NextG);
		if (pool.m_list[i]->ValidR()) {
			evaluate_conn(pool.m_list[i]->m_NextR);
		}

		// --- 4. 重なり判定 (既存ロジックがある場合) ---
		for (int j = i + 1; j < n; ++j) {
			if (state[j].x == POS_INVALID) continue;
			if (state[i].x == state[j].x && state[i].y == state[j].y) {
				energy += PENALTY_OVERLAP;
			}
		}
	}

	return energy;
}

// 線分(p1-p2)と(p3-p4)が交差するか判定（端点の共有は交差とみなさない）
bool is_intersect(Pos p1, Pos p2, Pos p3, Pos p4) {
	auto ccw = [](Pos a, Pos b, Pos c) {
		long long val = (long long)(b.x - a.x) * (c.y - a.y) - (long long)(b.y - a.y) * (c.x - a.x);
		if (val == 0) return 0; // 共線
		return (val > 0) ? 1 : 2; // 時計回り or 反時計回り
	};

	int d1 = ccw(p1, p2, p3);
	int d2 = ccw(p1, p2, p4);
	int d3 = ccw(p3, p4, p1);
	int d4 = ccw(p3, p4, p2);

	// 標準的な線分交差判定
	if (d1 != d2 && d3 != d4) return true;
	return false;
}

// 単一のエッジ(u, v)に対して、他のチップkがその線分上に居座っていないか（貫通）を判定
bool is_penetrating(int u, int v, const std::vector<Pos>& current_state) {
	Pos p1 = current_state[u];
	Pos p2 = current_state[v];

	// 水平接続のチェック
	if (p1.y == p2.y) {
		int min_x = std::min(p1.x, p2.x);
		int max_x = std::max(p1.x, p2.x);
		for (int k = 0; k < (int)current_state.size(); ++k) {
			if (k == u || k == v) continue;
			if (current_state[k].y == p1.y && current_state[k].x > min_x && current_state[k].x < max_x) {
				return true; // チップkが水平線上に存在する
			}
		}
	}
	// 垂直接続のチェック
	else if (p1.x == p2.x) {
		int min_y = std::min(p1.y, p2.y);
		int max_y = std::max(p1.y, p2.y);
		for (int k = 0; k < (int)current_state.size(); ++k) {
			if (k == u || k == v) continue;
			if (current_state[k].x == p1.x && current_state[k].y > min_y && current_state[k].y < max_y) {
				return true; // チップkが垂直線上に存在する
			}
		}
	}
	return false;
}

// 現在の全チップ配置において「交差」または「貫通」があるか一括判定（ハード制約用）
bool CarnageSA::is_invalid_layout(const std::vector<Pos>& current_state) {
	const int n = (int)current_state.size();
	struct Edge { int u, v; };
	std::vector<Edge> edges;

	// 1. 全接続（エッジ）をリストアップ
	for (int i = 0; i < n; ++i) {
		auto add_edge = [&](UINT next) {
			if (next != IDX_EXIT && next != IDX_NONE && next < (UINT)n) {
				edges.push_back({ i, (int)next });
			}
		};
		if(state[i].x != POS_INVALID){
			add_edge(pool.m_list[i]->m_NextG);
			if (pool.m_list[i]->ValidR()) add_edge(pool.m_list[i]->m_NextR);
		}
	}

	// 2. 貫通チェック
	for (const auto& edge : edges) {
		if (is_penetrating(edge.u, edge.v, current_state)) return true;
	}

	// 3. 交差チェック
	for (size_t i = 0; i < edges.size(); ++i) {
		for (size_t j = i + 1; j < edges.size(); ++j) {
			int u1 = edges[i].u, v1 = edges[i].v;
			int u2 = edges[j].u, v2 = edges[j].v;
			// 端点を共有（接続されている）チップ同士の線は交差判定から除外
			if (u1 == u2 || u1 == v2 || v1 == u2 || v1 == v2) continue;
			if (is_intersect(current_state[u1], current_state[v1], current_state[u2], current_state[v2])) return true;
		}
	}

	return false;
}

bool CarnageSA::cleanup_gotos() {
	bool total_changed = false;

	for (int target = 0; target < (int)pool.m_list.size(); ++target) {
		if (!pool[target] || pool[target]->m_Id.get() != CHIPID_GOTO) continue;
		if (state[target].x == POS_INVALID) continue;

		// 1. このGotoを指している全ての親（接続元）を特定
		struct Connection { int idx; bool is_r; };
		std::vector<Connection> parents;
		for (int i = 0; i < (int)pool.m_list.size(); ++i) {
			if (!pool[i] || state[i].x == POS_INVALID) continue;
			if (pool[i]->m_NextG == (UINT)target) parents.push_back({i, false});
			if (pool[i]->ValidR() && pool[i]->m_NextR == (UINT)target) parents.push_back({i, true});
		}

		if (parents.empty()) {
			state[target] = { POS_INVALID, POS_INVALID };
			total_changed = true;
			continue;
		}

		// 2. バックアップ
		UINT next_of_goto = pool[target]->m_NextG;
		Pos original_goto_pos = state[target];
		std::vector<UINT> original_conns;
		for(auto& p : parents) {
			original_conns.push_back(p.is_r ? pool[p.idx]->m_NextR : pool[p.idx]->m_NextG);
		}
		
		// 3. 全ての親をバイパス先に繋ぎ変え、Gotoを削除
		for (auto& p : parents) {
			if (p.is_r) pool[p.idx]->m_NextR = next_of_goto;
			else        pool[p.idx]->m_NextG = next_of_goto;
		}
		state[target] = { POS_INVALID, POS_INVALID };

		// 4. バリデーション (引数に state を渡す)
		if (is_invalid_layout(state)) {
			// ロールバック
			for (size_t k = 0; k < parents.size(); ++k) {
				if (parents[k].is_r) pool[parents[k].idx]->m_NextR = original_conns[k];
				else                 pool[parents[k].idx]->m_NextG = original_conns[k];
			}
			state[target] = original_goto_pos;
		} else {
			total_changed = true;
		}
	}
	
	return total_changed;
}

void CarnageSA::run() {
	double current_energy = calculate_energy();
	double best_energy = current_energy;
	std::vector<Pos> best_state = state;
	bool bUpdateBest = false;

	double T = 5000.0;
	const double alpha = 0.999995;
	const int iterations = 2000000;

	std::uniform_real_distribution<double> dist_u(0.0, 1.0);
	std::uniform_int_distribution<int> dist_rel(-1, 1);
	std::uniform_int_distribution<int> dist_abs(0, 14);

	printf("Starting SA... Initial Energy: %.2f\n", current_energy);

	for (int step = 0; step < iterations; ++step) {
		// 500回に1回、全てのGotoに対してバイパスと削除を試みる
		if (step % 512 == 0) {
			if (cleanup_gotos()) {
				best_energy = current_energy = calculate_energy();
				best_state = state;
				bUpdateBest = true;
			}
		}
		
		int target;
		do{
			std::uniform_int_distribution<int> dist_idx(0, (int)pool.m_list.size() - 1);
			target = dist_idx(this->gen);
		}while(state[target].x == POS_INVALID);

		// --- 通常の移動処理 ---
		Pos old_pos = state[target];
		if (dist_u(this->gen) < 0.9) {
			state[target].x += dist_rel(this->gen);
			state[target].y += dist_rel(this->gen);
		} else {
			state[target].x = dist_abs(this->gen);
			state[target].y = dist_abs(this->gen);
		}

		bool overlap = false;
		for (int i = 0; i < (int)state.size(); ++i) {
			if (i == target || state[i].x == POS_INVALID) continue;
			if (state[i].x == state[target].x && state[i].y == state[target].y) {
				overlap = true; break;
			}
		}

		if (overlap || is_invalid_layout(state)) {
			state[target] = old_pos;
			--step;
			continue;
		}

		double next_energy = calculate_energy();
		double delta = next_energy - current_energy;

		if (delta < 0 || exp(-delta / T) > dist_u(this->gen)) {
			current_energy = next_energy;
			if (current_energy < best_energy) {
				best_energy = current_energy;
				best_state = state;
				bUpdateBest = true;
			}
		} else {
			state[target] = old_pos;
		}

		T *= alpha;
		if (step % 100000 == 0){
			printf("Step: %7d, T: %7.2f, Energy: %10.2f, Best: %10.2f\n", step, T, current_energy, best_energy);
			if(bUpdateBest && m_svg_file){
				OutputSvg(m_svg_file, best_state);
				bUpdateBest = false;
			}
		}
		
		if(current_energy < 0.001) break;
	}
	state = std::move(best_state);
	if(m_svg_file)OutputSvg(m_svg_file, state);
	finalize();
}

void CarnageSA::finalize() {
	const int n = (int)pool.m_list.size();

	// 1. 削除された GoTo チップのメモリ解放と無効化
	for (int i = 0; i < n; ++i) {
		if (state[i].x == POS_INVALID) {
			// 安全に削除
			if (pool.m_list[i] != nullptr) {
				delete pool.m_list[i];
				pool.m_list[i] = nullptr;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////

void CarnageSA::OutputSvg(const char* filename, const std::vector<Pos>& state_disp) {
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
    if (pool.m_start < (UINT)state_disp.size() && state_disp[pool.m_start].x != POS_INVALID) {
        double x = state_disp[pool.m_start].x * CELL_SIZE + CELL_SIZE / 2.0;
        double y = state_disp[pool.m_start].y * CELL_SIZE + CELL_SIZE / 2.0;
        // 1マス上にある仮想チップからの接続として描画
        draw_physical_edge(x, y - CELL_SIZE, x, y, "#28a745");
    }

    // 4. チップ本体の描画
    for (int i = 0; i < (int)state_disp.size(); ++i) {
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
    for (int i = 0; i < (int)state_disp.size(); ++i) {
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

            if (next_idx < state_disp.size() && state_disp[next_idx].x != POS_INVALID) {
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
// C との命名被り回避

#define if(cc)		if_statement(cc);
#define elseif(cc)	elseif_statement(cc);
#define else		else_statement();
#define endif		endif_statement();

#define loop		loop_statement();
#define endloop		loopend_statement();
#define break		break_statement()

#define exit		okecc_exit
#define rand		okecc_rand

//////////////////////////////////////////////////////////////////////////////

void chip_main(void){
	// 格闘
	if(target_z() <= 6 && is_target_direction(0, 160) && target_distance() <= 30)
		fight();
		exit();
	endif
	
	// 冷却
	if(heat() >= 65)
		if(option_num(1))
			option(1);
		else
			option(2);
		endif
		
		if(heat() >= 70) option(3); endif
	endif
	
	lockon(0, 512, 320, OKE_ALL);
	
	if(barrier_height(0, 128, 20) >= 3 || projectile_num(0, 32, 160, P_ALL) || friendly_num(0, 128, 20, OKE_ALL))
		turn_left();
	endif
	
	// 方向転換
	if(barrier_height(0, 160, 40) >= 24 || target_distance() >= 140)
		if(is_target_direction(-128, 256))
			turn_left();
		else
			turn_right();
		endif
		exit();
	endif
	
	// 前進
	move_forward();
	
	if(target_distance() >= 50) exit(); endif
	
	// 飛行型には積極的には撃たない
	if(target_z() >= 6 || heat() >= 50) exit(); endif
	
	loop
		B = ammo_num(1);
		if(A != B || !is_self_firing()) break; endif
	endloop
	
	fire(0, 448, 160, OKE_ALL, 3, 1);
	fire(0, 448, 160, OKE_ALL, 1, 1);
	A = ammo_num(1);
	
	// 140m 以上離れた敵にはミサイルを打ちやすくする
	if(target_distance() >= 140)
		D = 1;
	endif
	
	B = time();
	C = ch_receive(1);
	
	// ミサイルタイマを過ぎるか，破損が多ければミサイルを撃つ
	if(!(B >= C || damage() >= 60 || D)) exit(); endif
	
	// ミサイル射撃
	ch_send(1, B += 3);
	
	if(ammo_num(2)) fire(0, 512, 320, OKE_ALL, 1, 3); endif
	wait_ae();
	if(ammo_num(3)) fire(0, 512, 320, OKE_ALL, 3, 3); endif
	
#if 0
	option(1);
	
	loop
		if(A > B) option(1); break; endif
		option(2);
	endloop
	
	option(3);
#endif
#if 0
	option(1);
	
	if(time() >= 60)
		F = time();
	endif
	
	if(damage() >= 50)
		lockon(128, 128, 160, OKE_ALL);
		C = heat();
	endif
	
	if(target_distance() <= 160)
		option(2);
	endif
	
	if(is_target_direction(-128, 128))
		fire_target(1, 8);
	endif
	
	if(target_y() >= 0)
		F = target_y();
	endif
	
	if(self_z() >= 0)
		B = self_z();
	endif
	
	if(is_self_stop())
		option(1);
	endif
	if(is_target_stun())
		get_target_direction(A, F);
	endif
	if(enemy_num(0, 416, 160, OKE_ALL))
		A = friendly_num();
	endif
	D = rand();
	B += F;
	if(F <= 2)
		F += 255;
	endif
	if(B != A)
		A = 0;
		(B = time()) += 3;
	endif
	fire_direction(0, 0, 1, 1);
	fire_direction(A, B, 1, 1);
	clamp(C, -127, 127);
	if(is_rand(49, 50))
		sound(1, 1);
	endif
	if(is_mappoint(-128, 128, 320, 1, 1))
		sound(5, 5);
	endif
	B = ch_receive(7);
	ch_send(1, F);
	sub(2);
#endif
#if 0
	nop();
	
	if(A > B)
		option(1);
	endif
	if(A < B)
		option(1);
	endif
	if(ammo_num(1))
		option(1);
	endif
	if(!ammo_num(1))
		option(1);
	endif
	
	if((enemy_num(0, 416, 160, OKE_ALL) >= 1))
		jump_forward();
	elseif(!(enemy_num(0, 416, 160, OKE_ALL) >= 1))
		jump_forward();
	elseif(ammo_num(1) >= 1)
		action1();
	else
		fire_target(1, 1);
	endif
	nop();
#endif

#if 0
	for(int i = 0; i < 1; ++i){
		if(enemy_num(0, 416, 160, OKE_ALL))
			if(enemy_num(0, 128, 320, OKE_HOVER) && friendly_num(0, 128, 320, OKE_ALL))
				turn_right();
				jump_backward();
			else
				move_forward();
			endif
			action1();
			end();
		endif
		guard();
		fire(0, 416, 160, OKE_HOVER, 1, 8);
		fire(128, 20, 1, 8);
		
		if(option_num(1) >= 2)
			if(barrier_height(0, 32, 160) >= 24)
				if(projectile_num(0, 32, 160, P_HI_V))
					option(1);
				endif
			endif
		endif
		if(ammo_num(2) > 20)
			B = ammo_num(2);
		endif
		if(ammo_num(1))
			B = ammo_num(1);
		endif
		C = heat();
		A = damage();
	}
#endif
#if 0
	if(enemy_num(0, 416, 160, OKE_ALL))
		if(enemy_num(0, 128, 320, OKE_HOVER) && friendly_num(0, 128, 320, OKE_ALL))
			turn_right();
			jump_backward();
		else
			move_forward();
		endif
		action1();
		end();
	endif
	guard();
	fire(0, 416, 160, OKE_HOVER, 1, 8);
	fire(128, 20, 1, 8);
	
	if(option_num(1) >= 2)
		if(barrier_height(0, 32, 160) >= 24)
			if(projectile_num(0, 32, 160, P_HI_V))
				option(1);
			endif
		endif
	endif
	if(ammo_num(2) > 20)
		B = ammo_num(2);
	endif
	if(ammo_num(1))
		B = ammo_num(1);
	endif
	C = heat();
	A = damage();
#endif
#if 0
	if(
		enemy_num(0, 416, 160, OKE_ALL) >= 1 && enemy_num(16, 416, 160, OKE_ALL) >= 1 ||
		enemy_num(32, 416, 160, OKE_ALL) >= 1 && enemy_num(64, 416, 160, OKE_ALL) >= 1
	)
		option(1);
	else
		option(3);
	endif
#endif
#if 0
		option(1);
		if(
			(enemy_num(0, 416, 160, OKE_ALL) >= 1 && enemy_num(16, 416, 160, OKE_ALL) >= 1) ||
			(enemy_num(0, 416, 160, OKE_ALL) >= 1 && enemy_num(16, 416, 160, OKE_ALL)) ||
			(enemy_num(0, 416, 160, OKE_ALL) && enemy_num(16, 416, 160, OKE_ALL) >= 1) ||
			(enemy_num(0, 416, 160, OKE_ALL) && enemy_num(16, 416, 160, OKE_ALL)) ||
			(enemy_num(0, 416, 160, OKE_ALL)) ||
			(ammo_num(1) >= 1) ||
			(ammo_num(1)) ||
			(ammo_num(1) && ammo_num(1))
		)
			option(1);
		else
			A = ammo_num(1);
		endif
#endif
#if 0
	if(ammo_num(1))
		option(1);
	else
		jump_forward();
		jump_forward();
		jump_forward();
	endif
	
	if(ammo_num(1))
		option(1);
		option(1);
	else
		jump_forward();
		jump_forward();
		jump_forward();
		jump_forward();
		jump_forward();
	endif
	
	if(ammo_num(1))
		option(1);
	else
		jump_forward();
	endif

	if(ammo_num(1))
		jump_forward();
		jump_forward();
		jump_forward();
	endif

	if(ammo_num(1))
		jump_forward();
		jump_forward();
	endif

	if(ammo_num(1))
		jump_forward();
	endif
	
	jump_backward();
#endif
}

//////////////////////////////////////////////////////////////////////////////

int main(void){
	g_pCurTree = new CChipTree;
	g_pCurChipPool = new CChipPool(15, 15);
	
	try{
		chip_main();
	}catch(const OkeccError& e){
		puts(e.what());
		return 0;
	}
	
	g_pCurTree->AddToG(IDX_EXIT);
	g_pCurChipPool->m_start = g_pCurTree->m_start;
	g_pCurChipPool->dump();
	
	// Goto 最適化
	g_pCurChipPool->CleanupGoto();
	
	printf("Number of chip(s): %d\n", (UINT)g_pCurChipPool->m_list.size());
	g_pCurChipPool->dump();
	
	CarnageSA sa(*g_pCurChipPool, "chip.svg");
	sa.run();
	sa.OutputSvg("chip.svg", sa.get_result());

	return 0;
}
