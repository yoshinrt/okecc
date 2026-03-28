#define _CRT_SECURE_NO_WARNINGS

#include <algorithm>
#include <cassert>
#include <climits>
#include <cmath>
#include <concepts>
#include <cstdlib>
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
	
	UINT GetRaw(void){return m_value;}
	void SetRaw(uint32_t bin){m_value = bin & ((1 << width) - 1);}
	
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
	
	virtual uint32_t GetBinary(void){
		return
			(m_Id.GetRaw() << 26) |
			(m_RawG.GetRaw() << 23);
	}
	
	virtual void SetBinary(uint32_t bin){
		m_Id.SetRaw(bin >> 26);
		m_RawG.SetRaw(bin >> 23);
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
	
	virtual uint32_t GetBinary(void){
		return
			CChip::GetBinary() |
			(m_RawR.GetRaw() << 20);
	}
	
	virtual void SetBinary(uint32_t bin){
		CChip::SetBinary(bin);
		m_RawR.SetRaw(bin >> 20);
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

static CCond option_num(
	int weapon,
	LastLocationArg
){
	LastLocation();
	return CCond(new CChipAmmoNum(weapon + 4));
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

static CCond barrier_height(
	int angleCenter,
	int angleRange,
	int distance,
	LastLocationArg
){
	LastLocation();
	return CCond(new CChipBarrier(angleCenter, angleRange, distance));
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
	return CCond(new CChipIsRand(num1, num2)).GetCChipTree();
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
	ScaledInt<1>			m_enemy;
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

static CCond target_distance(
	LastLocationArg
){
	LastLocation();
	return CCond(new CChipTgtDistance());
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
	
	ScaledInt<1>			m_self_tgt;
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
	return CCond(new CChipTgtAction(self_tgt, param)).GetCChipTree();
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

//////////////////////////////////////////////////////////////////////////////
// if - else - endif

class CBlockInfo;
CBlockInfo *g_pCurBlockInfo;

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

void if_statement(const CChipTree& cc, LastLocationArg, bool BlockStart = true){
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

void if_statement(const CCond& chip, LastLocationArg){if_statement(chip.GetCChipTree(), location);}
void if_statement(const CChipVal&  chip, LastLocationArg){if_statement(chip >= 1, location);}
void if_statement(const CChipVar&  chip, LastLocationArg){if_statement(chip != 0, location);}

void else_statement(
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

void elseif_statement(CChipTree &&cc, LastLocationArg){
	LastLocation();
	else_statement(location);
	if_statement(cc, location, false);
}

void elseif_statement(const CCond& chip, LastLocationArg){elseif_statement(chip.GetCChipTree(), location);}
void elseif_statement(const CChipVal&  chip, LastLocationArg){elseif_statement(chip >= 1, location);}
void elseif_statement(const CChipVar&  chip, LastLocationArg){elseif_statement(chip != 0, location);}

void endif_statement(
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

void loop_statement(LastLocationArg){
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

void loopend_statement(LastLocationArg){
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

void break_statement(LastLocationArg){
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
// 焼きなまし法

struct Pos {
	UINT x, y;
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
	std::vector<std::vector<UINT>> LinkList; // チップごとの接続リスト

	UINT GridWidth;
	UINT GridHeight;
	std::vector<Pos> state;
	std::mt19937 gen;
	const char *m_svg_file;

	// 8方向のベクトル定義 (N, NE, E, SE, S, SW, W, NW)
	inline static const int dx[] = { 0,  1,  1,  1,  0, -1, -1, -1};
	inline static const int dy[] = {-1, -1,  0,  1,  1,  1,  0, -1};
	
public:
	CarnageSA(CChipPool& p, const char *svg_file = nullptr)
		: pool(p), GridWidth(p.m_width), GridHeight(p.m_height), gen(std::random_device{}()), m_svg_file(svg_file){
		LinkList.resize(pool.size());
		initialize();
	}
	
	// 座標の正規化：最小値を (0, 0) に合わせる
	void normalize_coordinates() {
		int min_x = INT_MAX, min_y = INT_MAX;
		bool has_data = false;
	
		for (const auto& p : state) {
			if (p.x != INT_MAX) {
				min_x = std::min(min_x, (int)p.x);
				min_y = std::min(min_y, (int)p.y);
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
	void run();
	double calculate_energy(const std::vector<Pos>& state, const std::vector<UINT>& occ);
	void OutputSvg(const char* filename, const std::vector<Pos>& state_disp);
	
	const std::vector<Pos>& get_result() const { return state; }
};

void CarnageSA::initialize() {
	for (UINT i = 0; i < (UINT)pool.size(); ++i) {
		UINT y = i / GridWidth;
		UINT x = i % GridWidth;
		x = (y % 2 == 0) ? x : (GridWidth - 1 - x);
		state.push_back({x, y});
		
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

double CarnageSA::calculate_energy(const std::vector<Pos>& state, const std::vector<UINT>& occ_org) {
	double base_energy = 0.0;
	
	// ペナルティ
	const double distance_penalty	= 1.0;		// chip 距離
	const double overwrap_penalty	= 100.0;	// チップ間 path が他のチップを通過
	const double start_end_penalty	= 1000.0;	// start/end 枠からの距離
	
	const size_t N = state.size();
	
	auto occ = occ_org;

	// パラメータ（調整可能）

	for (UINT u = 0; u < (UINT)state.size(); ++u) {
		Pos p = state[u];
		CChip* chip = pool[u];

		// 物理制約
		if (!(p.x < GridWidth) || !(p.y < GridHeight)) base_energy += 5000.0;

		// Startチップ制約
		if (u == pool.m_start && p.y != 0) base_energy += p.y * start_end_penalty;
		
		// exit チップ制約
		if(pool[u]->m_NextG == IDX_EXIT || pool[u]->m_NextR == IDX_EXIT){
			base_energy += start_end_penalty * std::min(
				std::min(p.x, GridWidth - p.x - 1),
				std::min(p.y, GridHeight- p.y - 1)
			);
		}
		
		auto IsBrank = [&](int x, int y) -> bool {
			return occ[y * GridWidth + x] == IDX_NONE;
		};
		
		auto Occupy = [&](int x, int y){
			occ[y * GridWidth + x] = 0;
		};
		
		auto ChipE = [&](UINT ToIdx) -> double {
			// チップ距離
			Pos to = state[ToIdx];
			
			auto e = distance_penalty * (std::max(
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

void CarnageSA::run() {
	// 改良版 SA:
	// - 隣接スワップ中心（局所探索）
	// - 低確率で任意2点スワップ（global）とランダムジャンプ
	constexpr int max_iter = 8000000;
	constexpr double cooling = 0.999997;
	double T = 2000.0;
	
	constexpr UINT p_random_swap	= 1;	// ランダムスワップ
	constexpr UINT p_nearby_swap	= 5;	// 隣接スワップ
	constexpr UINT p_move_mid		= 50;	// 接続chip の真ん中に移動
	
	std::uniform_int_distribution<UINT>		dist_idx(0, (UINT)pool.size() - 1);
	std::uniform_int_distribution<int>		dist_dir(0, 7);
	std::uniform_int_distribution<UINT>		dist_prob(0,
		p_random_swap +
		p_nearby_swap +
		p_move_mid
	);
	std::uniform_int_distribution<int> dist_x(0, GridWidth - 1);
	std::uniform_int_distribution<int> dist_y(0, GridHeight - 1);

	// occupancy
	std::vector<UINT> occ(GridWidth * GridHeight, IDX_NONE);
	std::vector<UINT> next_occ(GridWidth * GridHeight, IDX_NONE);
	
	auto rebuild_occ = [&](const std::vector<Pos>& s) {
		std::fill(occ.begin(), occ.end(), IDX_NONE);
		
		for (UINT j = 0; j < (UINT)s.size(); ++j) {
			const Pos& pp = s[j];
			if (pp.x < GridWidth && pp.y < GridHeight){
				occ[pp.y * GridWidth + pp.x] = j;
			}
		}
	};
	rebuild_occ(state);
	
	auto dump_occ = [&](std::vector<UINT>& occ){
		printf("----------------\n");
		for(UINT u = 0; u < occ.size(); ++u){
			if(occ[u] == IDX_NONE) printf("-- ");
			else printf("%02d ", occ[u]);
			if(u % GridWidth == (GridWidth - 1)) printf("\n");
		}
	};
	
	double current_E	= calculate_energy(state, occ);
	auto best_state		= state;
	double best_E		= current_E;
	bool UpdateBest		= false;

	// move probabilities (base)
	// neighbor-swap が残りの確率
	auto next_state = state;

	// 1chip 移動
	auto SwapChipXY = [&](UINT ax, UINT ay, UINT bx, UINT by) {
		UINT achip = next_occ[ay * GridWidth + ax];
		UINT bchip = next_occ[by * GridWidth + bx];
		
		std::swap(next_occ[ay * GridWidth + ax], next_occ[by * GridWidth + bx]);
		
		if(achip != IDX_NONE){
			next_state[achip].x = bx;
			next_state[achip].y = by;
		}
		if(bchip != IDX_NONE){
			next_state[bchip].x = ax;
			next_state[bchip].y = ay;
		}
	};
	
	auto SwapChip = [&](UINT a, UINT b){
		SwapChipXY(
			next_state[a].x, next_state[a].y,
			next_state[b].x, next_state[b].y
		);
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
			int dir = dist_dir(gen);
			UINT nx = next_state[SrcIdx].x + dx[dir];
			UINT ny = next_state[SrcIdx].y + dy[dir];
			if (nx < GridWidth && ny < GridHeight){
				SwapChip(nx, ny);
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
			
			UINT n = LinkList[SrcIdx].size();
			dst_x = (dst_x + n / 2) / n;
			dst_y = (dst_y + n / 2) / n;

			auto p = next_state[SrcIdx];

			// 移動先が同じなら continue
			if (p.x == dst_x && p.y == dst_y){
				--iter;
				continue;
			}
			
			// start チップの場合は dst_y=0 に固定
			if (SrcIdx == pool.m_start) dst_y = 0;

			// Exit チップの場合は端に固定
			else if(pool[SrcIdx]->m_NextG == IDX_EXIT || pool[SrcIdx]->m_NextR == IDX_EXIT){
				UINT xdist = std::min(dst_x, GridWidth  - 1 - dst_x);
				UINT ydist = std::min(dst_y, GridHeight - 1 - dst_y);

				if (xdist < ydist) {
					dst_x = (dst_x < GridWidth / 2) ? 0 : (GridWidth - 1);
				} else {
					dst_y = (dst_y < GridHeight / 2) ? 0 : (GridHeight - 1);
				}
			}

			// 移動先が空きならそのまま移動
			if (next_occ[dst_y * GridWidth + dst_x] == IDX_NONE) {
				SwapChipXY(next_state[SrcIdx].x, next_state[SrcIdx].y, dst_x, dst_y);
				proposed = true;
			}else{
				// 一旦 chip を消す
				next_occ[p.y * GridWidth + p.x] = IDX_NONE;
				
				// 最も近い空きセルを探す
				UINT best_x;
				UINT best_y;
				UINT best_dist = INT_MAX;
	
				for(UINT y = 0; y < GridHeight; ++y){
					for(UINT x = 0; x < GridWidth; ++x){
						int occIdx = next_occ[y * GridWidth + x];
						if (occIdx == IDX_NONE) {
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
				
				next_occ[dst_y * GridWidth + dst_x] = SrcIdx;
				next_state[SrcIdx].x = dst_x;
				next_state[SrcIdx].y = dst_y;
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
		
		if (iter % 200000 == 0){
			//dump_occ(next_occ);
			printf("Step: %7d | T: %7.2f Energy: %8.2f | Best: %8.2f\n", iter, T, current_E, best_E);
			OutputSvg("z.svg", next_state);
			if(UpdateBest){
				OutputSvg(m_svg_file, state);
				UpdateBest = false;
			}
		}
	}

	state = best_state;
	OutputSvg(m_svg_file, state);	
	printf("Step: %7d | Energy: %.2f\n", iter, best_E);
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
#if 1
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
	D = target_z();
	
	if(
		barrier_height(0, 128, 20) >= 3 ||
		projectile_num(0, 32, 160, P_ALL) ||
		friendly_num(0, 128, 20, OKE_ALL) ||
		barrier_height(0, 160, 40) >= 24
	)
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
	
	// 近すぎる敵から距離を取る
	if(target_distance() <= 50) exit(); endif
	
	// 飛行型には積極的にはカノンを撃たない
	if(!(target_z() >= 6 && heat() >= 50))
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
	endif
	
	B = time();
	C = ch_receive(1);
	
	// ミサイルタイマを過ぎるか，破損が多ければミサイルを撃つ
	if(B >= C || damage() >= 60 || D)
		// ミサイル射撃
		ch_send(1, B += 3);
		
		if(ammo_num(2)) fire(0, 512, 320, OKE_ALL, 1, 3); endif
		wait_ae();
		if(ammo_num(3)) fire(0, 512, 320, OKE_ALL, 3, 3); endif
	endif
#endif
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
