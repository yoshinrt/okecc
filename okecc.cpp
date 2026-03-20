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
	"ALL",
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
	CHIPID_DET_OBSTACLE,
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
	CHIPID_IS_POS,
	CHIPID_IS_ACTION,
	CHIPID_SOUND,
	CHIPID_GET_STATUS,
	CHIPID_GET_POS,
	CHIPID_GET_TGT_DIR,
	CHIPID_GET_MISC_NUM,
	CHIPID_CALC,
	CHIPID_TRUNCATE,
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
	CChip(){
		m_id		= CHIPID_NULL;
	}

	virtual ~CChip(){}
	
	virtual std::string GetLayoutText(void){
		return "";
	}
	
	bool valid_r(void){return m_next_r < IDX_EXIT;}
	bool valid_g(void){return m_next_g < IDX_EXIT;}

	ScaledInt<6>	m_id;
	ScaledInt<3>	m_raw_g;
	ScaledInt<3>	m_raw_r;

	UINT			m_next_g = IDX_NONE;
	UINT			m_next_r = IDX_NONE;
};

class CChipCond : public CChip {
public:
	virtual ~CChipCond(){}
};

//////////////////////////////////////////////////////////////////////////////
// ゲームには存在しない仮チップ

class CChipGoto : public CChip {
public:
	CChipGoto(){
		m_id = CHIPID_GOTO;
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
		while(dst < m_list.size() && m_list[dst]->m_id.get() == CHIPID_GOTO) dst = m_list[dst]->m_next_g;

		// 最終地点までの Goto はすべて最終地点を指す
		while(idx != dst){
			UINT next = m_list[idx]->m_next_g;
			m_list[idx]->m_next_g = dst;
			idx = next;
		}

		return dst;
	}
};

CChipPool	g_ChipPool(15, 15);

//////////////////////////////////////////////////////////////////////////////
// Chip tree

class CChipTree {
public:

	UINT m_start	= IDX_NONE;	// 開始 chip
	UINT m_last_g	= IDX_NONE;	// 最後の緑矢印を出している chip
	UINT m_last_r	= IDX_NONE; // 最後の赤矢印を出している chip

	// g, r を update
	void set_g(UINT idx){
		if(m_start == IDX_NONE) m_start = idx;
		if(valid_g()) g_ChipPool[m_last_g]->m_next_g = idx;
		m_last_g = idx;
	}

	void set_r(UINT idx){
		if(m_start == IDX_NONE) m_start = idx;
		if(valid_r()) g_ChipPool[m_last_r]->m_next_r = idx;
		m_last_r = idx;
	}

	bool valid_start(void) const {return m_start  < IDX_EXIT;}
	bool valid_g    (void) const {return m_last_g < IDX_EXIT;}
	bool valid_r    (void) const {return m_last_r < IDX_EXIT;}

	// チップ単体からツリーに変換
	static CChipTree Chip2Tree(CChip *pchip){
		CChipTree tree;

		tree.m_start =
		tree.m_last_g = g_ChipPool.add(pchip);

		return tree;
	}

	// チップ単体からツリーに変換 (Condition chip)
	static CChipTree CondChip2Tree(CChip *pchip){
		CChipTree tree;

		tree.m_start =
		tree.m_last_g =
		tree.m_last_r = g_ChipPool.add(pchip);

		return tree;
	}

	// Tree にチップ追加
	CChipTree& add(CChip *pchip){
		UINT idx = g_ChipPool.add(pchip);		// チップ追加

		if(valid_start()){
			g_ChipPool[m_last_g]->m_next_g = idx;	// リスト後端に追加したチップをつなげる
		}else{
			m_start = idx;
		}
		m_last_g = idx;

		return *this;
	}
};

CChipTree *g_pCurTree;

CChipTree operator&&(const CChipTree& a, const CChipTree& b){
	CChipTree cc = a;

	// condition な tree か判定
	if(!cc.valid_r() || !b.valid_r()){
		throw OkeccError("Invalid use of && operator: Condition chip expected.");
	}

	// this.r -> b.start を指す
	cc.set_r(b.m_start);

	// False 側: GOTO を生成し this.g, b.g, tree.g はそれを指す
	UINT idx = g_ChipPool.add(new CChipGoto);
	cc.set_g(idx);
	g_ChipPool[b.m_last_g]->m_next_g = idx;

	// True 側: tree.r は b.r になる
	cc.m_last_r = b.m_last_r;

	return cc;
}

CChipTree operator&&(CChipCond& chip, const CChipTree& a){
	return CChipTree::CondChip2Tree(&chip) && a;
}

CChipTree operator&&(const CChipTree& a, CChipCond& chip){
	return a && CChipTree::CondChip2Tree(&chip);
}

CChipTree operator&&(CChipCond& a, CChipCond& b){
	return CChipTree::CondChip2Tree(&a) && CChipTree::CondChip2Tree(&b);
}

//////////////////////////////////////////////////////////////////////////////
// NOP

static constexpr UINT NOP_AE = 62;

class CChipNop : public CChip {
public:
	CChipNop(UINT param){
		m_id	= CHIPID_NOP;
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
		m_id	= CHIPID_MOVE;
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
		m_id	= CHIPID_TURN;
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
		m_id	= CHIPID_JUMP;
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
		m_id	= CHIPID_ACTION;
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
		m_id	= CHIPID_ALTITUDE;
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
		m_id			= CHIPID_FIRE_NEAREST;
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
		m_id		= CHIPID_FIRE_FIXED_DIR;
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

static void fire(
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

//////////////////////////////////////////////////////////////////////////////
// option

class CChipOption : public CChip {
public:
	CChipOption(UINT param){
		m_id	= CHIPID_OPTION;
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
// 近くの OKE を探索

class CChipOkeNum : public CChipCond {
public:
	static constexpr int OP_GE		= 0;
	static constexpr int OP_LE		= 1;
	static constexpr int ENEMY		= 0;
	static constexpr int FRIENDLY	= 1;

	CChipOkeNum(CChipOkeNum& src){
		*this = src;
	}

	virtual ~CChipOkeNum(){}

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
		m_id			= CHIPID_DET_OKE;
	}

	CChipTree operator>=(int num){
		m_num = num;
		m_operator = OP_GE;
		return CChipTree::CondChip2Tree(this);
	}

	CChipTree operator>(int num){
		m_num = num + 1;
		m_operator = OP_GE;
		return CChipTree::CondChip2Tree(this);
	}

	CChipTree operator<=(int num){
		m_num = num;
		m_operator = OP_LE;
		return CChipTree::CondChip2Tree(this);
	}

	CChipTree operator<(int num){
		m_num = num - 1;
		m_operator = OP_LE;
		return CChipTree::CondChip2Tree(this);
	}

	virtual std::string GetLayoutText(void){
		return
			std::format(
				"?{} {}m\n{}{}{}\n{},{}",
				(m_enemy.get() == ENEMY ? "敵" : "味方"), m_distance.get(),
				OkeTypeStr[m_type.get()], (m_operator.get() == OP_GE ? "≧" : "≦"), m_num.get(),
				m_angleCenter.get(), m_angleRange.get()
			);
	}
	
	ScaledInt<3>			m_type;
	ScaledInt<1>		 	m_enemy;
	ScaledInt<4, 20, 20>	m_distance;
	ScaledInt<5, 16, -240>	m_angleCenter;
	ScaledInt<4, 32, 32>	m_angleRange;
	ScaledInt<1>			m_operator;
	ScaledInt<2, 1, 1>		m_num;
};

static CChipOkeNum& oke_num(
	int angleCenter,
	int angleRange,
	int distance,
	int type,
	int enemy
){
	return *(new CChipOkeNum(angleCenter, angleRange, distance, type, enemy));
}

static CChipOkeNum& enemy_num(
	int angleCenter,
	int angleRange,
	int distance,
	int type,
	LastLocationArg
){
	LastLocation();
	return oke_num(angleCenter, angleRange, distance, type, CChipOkeNum::ENEMY);
}

static CChipOkeNum& friendly_num(
	int angleCenter,
	int angleRange,
	int distance,
	int type,
	LastLocationArg
){
	LastLocation();
	return oke_num(angleCenter, angleRange, distance, type, CChipOkeNum::FRIENDLY);
}

//////////////////////////////////////////////////////////////////////////////
// if - else - endif

std::vector<UINT>	g_BlockStack;

void if_statement(
	CChipTree &&cc,
	LastLocationArg
){
	LastLocation();

	// CurTree に if 条件のツリーを接続
	g_pCurTree->set_g(cc.m_start);

	// cc.r を GOTO に変換
	UINT idx = g_ChipPool.add(new CChipGoto);
	cc.set_r(idx);
	g_pCurTree->m_last_g = idx;

	// false 飛び先を push
	g_BlockStack.push_back(cc.m_last_g);
}

void if_statement(
	CChipCond &chip,
	LastLocationArg
){
	if_statement(CChipTree::CondChip2Tree(&chip), location);
}

void else_statement(
	LastLocationArg
){
	LastLocation();

	if(g_BlockStack.size() < 1){
		throw OkeccError("Unexpected ENDIF");
	}

	UINT idx = g_pCurTree->m_last_g;	// then 節の最後

	// else 節先頭は，BlockStack に積んでいた false 飛び先
	g_pCurTree->m_last_g = g_BlockStack.back(); g_BlockStack.pop_back();

	// then 節の最後を block statck に積む
	g_BlockStack.push_back(idx);
}

void endif_statement(
	LastLocationArg
){
	LastLocation();

	if(g_BlockStack.size() < 1){
		throw OkeccError("Unexpected ENDIF");
	}

	// 合流 GOTO 生成
	UINT merge = g_ChipPool.add(new CChipGoto);
	g_pCurTree->set_g(merge);

	// false 条件の飛び先合流
	UINT idx = g_BlockStack.back(); g_BlockStack.pop_back();
	g_ChipPool[idx]->m_next_g = merge;
}

#define IF(cc)	if_statement(cc);
#define ELSE	else_statement();
#define ENDIF	endif_statement();

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

	// 8方向のベクトル定義 (N, NE, E, SE, S, SW, W, NW)
	inline static const int dx[] = { 0,  1,  1,  1,  0, -1, -1, -1};
	inline static const int dy[] = {-1, -1,  0,  1,  1,  1,  0, -1};
	
public:
    CarnageSA(CChipPool& p)
        : pool(p), grid_width(p.m_width), grid_height(p.m_height), gen(std::random_device{}()) {
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
	void print_layout_svg(const char* filename);
	bool cleanup_gotos();
	
	const std::vector<Pos>& get_result() const { return state; }
};

std::vector<int> CarnageSA::calculate_max_distances() {
    const int num_chips = (int)pool.size();
    // -1 は「まだ EXIT までの距離が不明」または「到達不能」を意味する
    std::vector<int> dists(num_chips, -1);

    // 1. 初期状態：EXIT に直接つながっているチップの距離を 1 とする
    for (int i = 0; i < num_chips; ++i) {
        if (pool[i]->m_next_g == IDX_EXIT || 
           (pool[i]->valid_r() && pool[i]->m_next_r == IDX_EXIT)) {
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

            update(pool[i]->m_next_g);
            if (pool[i]->valid_r()) {
                update(pool[i]->m_next_r);
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
            if (pool.m_list[i]->m_next_g == IDX_EXIT || 
               (pool.m_list[i]->valid_r() && pool.m_list[i]->m_next_r == IDX_EXIT)) {
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
                update(i, pool.m_list[i]->m_next_g);
                if (pool.m_list[i]->valid_r()) update(i, pool.m_list[i]->m_next_r);
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
        if (f->m_next_g == old_to) f->m_next_g = new_to;
        else if (f->valid_r() && f->m_next_r == old_to) f->m_next_r = new_to;
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
            bool has_r = (curr < (UINT)original_num_chips) ? pool.m_list[curr]->valid_r() : false;
            int dg = (curr < (UINT)original_num_chips) ? get_d(pool.m_list[curr]->m_next_g) : -100;
            int dr = (curr < (UINT)original_num_chips && has_r) ? get_d(pool.m_list[curr]->m_next_r) : -100;
            curr = (dg >= dr) ? pool.m_list[curr]->m_next_g : pool.m_list[curr]->m_next_r;
        }
        return IDX_NONE;
    };

    auto count_path_chips = [&](UINT start, UINT goal) -> int {
        if (start == goal) return 0;
        int count = 0;
        UINT curr = start;
        while (curr != goal && curr != IDX_EXIT && curr != IDX_NONE && count < 256) {
            count++;
            bool has_r = (curr < (UINT)original_num_chips) ? pool.m_list[curr]->valid_r() : false;
            int dg = (curr < (UINT)original_num_chips) ? get_d(pool.m_list[curr]->m_next_g) : -100;
            int dr = (curr < (UINT)original_num_chips && has_r) ? get_d(pool.m_list[curr]->m_next_r) : -100;
            curr = (dg >= dr) ? pool.m_list[curr]->m_next_g : pool.m_list[curr]->m_next_r;
        }
        return count;
    };

    // --- 2. 再帰配置関数 ---
    auto place_path = [&](auto self, UINT start_idx, int start_x, int start_y) -> void {
        struct BranchTask { UINT parent; UINT target; };
        std::vector<BranchTask> pending_tasks;
        
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
                if (chip->valid_r()) {
                    int dg = get_d(chip->m_next_g);
                    int dr = get_d(chip->m_next_r);
                    UINT other;
                    if (dg >= dr) {
                        next_node = chip->m_next_g;
                        other = chip->m_next_r;
                    } else {
                        next_node = chip->m_next_r;
                        other = chip->m_next_g;
                    }
                    if (other != IDX_EXIT && other != IDX_NONE) {
                        // 未処理の分岐として保存
                        pending_tasks.push_back({curr, other});
                    }
                } else {
                    next_node = chip->m_next_g;
                }
            } else {
                next_node = pool.m_list[curr]->m_next_g;
            }
            curr = next_node;
            cur_x++;
        }

        // STEP 2: 保存した分岐タスクの処理
        for (auto& task : pending_tasks) {
            // 他のパス（異なる y 座標）ですでに配置済みなら合流とみなしてスキップ
            if (state[task.target].x != INT_MAX && state[task.target].y != start_y) {
                continue; 
            }

            UINT join_node = find_join_node(task.target);
            if (join_node == IDX_NONE) continue;

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

            int target_bx = state[join_node].x - 1;
            int path_len = count_path_chips(task.target, join_node);

            if (path_len == 0) {
                // 即時合流(0 chip): コの字迂回 GoTo 生成
                UINT g1 = add_goto_chip(bx, target_y);
                UINT g2 = add_goto_chip(std::max(bx, target_bx), target_y);
                modify_next(task.parent, task.target, g1);
                pool.m_list[g1]->m_next_g = g2;
                pool.m_list[g2]->m_next_g = join_node;
                continue; 
            } else if (path_len == 1) {
                // 1 chip: ターゲットチップ自体の配置後に GoTo で合流を補助
                UINT g1 = add_goto_chip(std::max(bx + 1, target_bx), target_y);
                pool.m_list[g1]->m_next_g = join_node;
                modify_next(task.target, join_node, g1);
            }

            // 枝パスの再帰配置
            self(self, task.target, bx, target_y);
        }
    };

    // 3. 実行
    place_path(place_path, 0, 0, 0);
    normalize_coordinates();
}

UINT CarnageSA::find_join_node(UINT start_idx, const std::vector<int>& dists) {
    UINT curr = start_idx;
    const int num_chips = (int)pool.size();

    while (curr != IDX_EXIT && curr != IDX_NONE) {
        if (state[curr].x != INT_MAX) return curr; // 合流点発見

        bool has_r = pool[curr]->valid_r();
        int d_g = (pool[curr]->m_next_g == IDX_EXIT) ? 0 : 
                  (pool[curr]->m_next_g < (UINT)num_chips ? dists[pool[curr]->m_next_g] : -100);
        int d_r = has_r ? ((pool[curr]->m_next_r == IDX_EXIT) ? 0 : 
                  (pool[curr]->m_next_r < (UINT)num_chips ? dists[pool[curr]->m_next_r] : -100)) : -100;

        curr = (d_g >= d_r) ? pool[curr]->m_next_g : pool[curr]->m_next_r;
    }
    return IDX_NONE;
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

        evaluate_conn(pool.m_list[i]->m_next_g);
        if (pool.m_list[i]->valid_r()) {
            evaluate_conn(pool.m_list[i]->m_next_r);
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
	        add_edge(pool.m_list[i]->m_next_g);
	        if (pool.m_list[i]->valid_r()) add_edge(pool.m_list[i]->m_next_r);
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
        if (!pool[target] || pool[target]->m_id.get() != CHIPID_GOTO) continue;
        if (state[target].x == POS_INVALID) continue;

        // 1. このGotoを指している全ての親（接続元）を特定
        struct Connection { int idx; bool is_r; };
        std::vector<Connection> parents;
        for (int i = 0; i < (int)pool.m_list.size(); ++i) {
            if (!pool[i] || state[i].x == POS_INVALID) continue;
            if (pool[i]->m_next_g == (UINT)target) parents.push_back({i, false});
            if (pool[i]->valid_r() && pool[i]->m_next_r == (UINT)target) parents.push_back({i, true});
        }

        if (parents.empty()) {
            state[target] = { POS_INVALID, POS_INVALID };
            total_changed = true;
            continue;
        }

        // 2. バックアップ
        UINT next_of_goto = pool[target]->m_next_g;
        Pos original_goto_pos = state[target];
        std::vector<UINT> original_conns;
        for(auto& p : parents) {
            original_conns.push_back(p.is_r ? pool[p.idx]->m_next_r : pool[p.idx]->m_next_g);
        }
        
        // 3. 全ての親をバイパス先に繋ぎ変え、Gotoを削除
        for (auto& p : parents) {
            if (p.is_r) pool[p.idx]->m_next_r = next_of_goto;
            else        pool[p.idx]->m_next_g = next_of_goto;
        }
        state[target] = { POS_INVALID, POS_INVALID };

        // 4. バリデーション (引数に state を渡す)
        if (is_invalid_layout(state)) {
            // ロールバック
            for (size_t k = 0; k < parents.size(); ++k) {
                if (parents[k].is_r) pool[parents[k].idx]->m_next_r = original_conns[k];
                else                 pool[parents[k].idx]->m_next_g = original_conns[k];
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

    double T = 5000.0;
    const double alpha = 0.999995;
    const int iterations = 4000000;

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
            }
        } else {
            state[target] = old_pos;
        }

        T *= alpha;
        if (step % 100000 == 0){
        	printf("Step: %7d, T: %7.2f, Energy: %10.2f, Best: %10.2f\n", step, T, current_energy, best_energy);
			print_layout_svg("chip.svg");
        }
        
        if(current_energy < 0.001) break;
    }
    state = std::move(best_state);
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

void CarnageSA::print_layout_svg(const char* filename) {
    const int CHIP_SIZE = 75;    // チップサイズ 1.5倍
    const int GAP = 15;          // 隙間 15px
    const int CELL_SIZE = CHIP_SIZE + GAP; 
    const int OFFSET = (CELL_SIZE - CHIP_SIZE) / 2;

    FILE* fp = fopen(filename, "w");
    if (!fp) return;

    // 1. 範囲計算 (メンバ変数 state, pool を直接使用)
    int min_x = INT_MAX, min_y = INT_MAX;
    int max_x = INT_MIN, max_y = INT_MIN;
    bool has_valid_chip = false;

    for (int i = 0; i < (int)state.size(); ++i) {
        if (pool[i] == nullptr || state[i].x == POS_INVALID) continue;
        has_valid_chip = true;
        min_x = std::min(min_x, state[i].x);
        min_y = std::min(min_y, state[i].y);
        max_x = std::max(max_x, state[i].x);
        max_y = std::max(max_y, state[i].y);
    }
    
    if (!has_valid_chip) { min_x = min_y = 0; max_x = max_y = 5; }

    int grid_w = (max_x - min_x + 1);
    int grid_h = (max_y - min_y + 1);
    int vb_width = grid_w * CELL_SIZE + 40;
    int vb_height = grid_h * CELL_SIZE + 40;
    int vb_x = min_x * CELL_SIZE - 20;
    int vb_y = min_y * CELL_SIZE - 20;

    // 2. SVGヘッダーとマーカー定義
    fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(fp, "<svg width=\"%d\" height=\"%d\" viewBox=\"%d %d %d %d\" xmlns=\"http://www.w3.org/2000/svg\">\n", 
            vb_width, vb_height, vb_x, vb_y, vb_width, vb_height);
    
    fprintf(fp, "  <defs>\n");
    fprintf(fp, "    <marker id=\"arrow\" markerWidth=\"10\" markerHeight=\"10\" refX=\"9\" refY=\"3\" orient=\"auto\" markerUnits=\"strokeWidth\">\n");
    fprintf(fp, "      <path d=\"M0,0 L0,6 L9,3 z\" fill=\"context-stroke\" />\n");
    fprintf(fp, "    </marker>\n");
    fprintf(fp, "  </defs>\n");

    // 背景
    fprintf(fp, "  <rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" fill=\"#f8f9fa\" />\n", vb_x, vb_y, vb_width, vb_height);

    // 3. チップ（箱とテキスト）の描画
    for (int i = 0; i < (int)state.size(); ++i) {
        if (pool[i] == nullptr || state[i].x == POS_INVALID) continue;

        int x = state[i].x * CELL_SIZE + OFFSET;
        int y = state[i].y * CELL_SIZE + OFFSET;
        int centerX = x + CHIP_SIZE / 2;

        // goto チップは薄いグレー背景、それ以外は白
        const char* fill_color = (pool.m_list[i]->m_id.get() == CHIPID_GOTO) ? "#e9ecef" : "white";

        // 箱
        fprintf(fp, "  <rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" rx=\"4\" fill=\"%s\" stroke=\"#343a40\" stroke-width=\"2\" />\n",
                x, y, CHIP_SIZE, CHIP_SIZE, fill_color);
        
        // テキストの描画
        fprintf(fp, "  <text x=\"%d\" y=\"%d\" font-family=\"Meiryo, sans-serif\" text-anchor=\"middle\">\n", centerX, y + 25);

        std::string txt = pool[i]->GetLayoutText() + std::format("\n#{}", i);
        if (txt.empty()) {
            // テキスト空時はTypeIDを表示
            fprintf(fp, "    <tspan x=\"%d\" dy=\"1.2em\" font-size=\"10\" fill=\"#6c757d\">Type:%u</tspan>\n", centerX, pool[i]->m_id.get());
        } else {
            // \n で改行
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
        }
        fprintf(fp, "  </text>\n");
    }

    // 4. 接続線（エッジ）の描画（最後に描画することで最前面へ）
    auto draw_edge = [&](int from_idx, UINT to_idx, const char* color) {
        if (to_idx == IDX_NONE || pool[from_idx] == nullptr || state[from_idx].x == POS_INVALID) return;
        
        Pos p1 = state[from_idx];
        double x1 = p1.x * CELL_SIZE + CELL_SIZE / 2.0;
        double y1 = p1.y * CELL_SIZE + CELL_SIZE / 2.0;

        // EXITへの線
        if (to_idx == IDX_EXIT) {
            fprintf(fp, "  <line x1=\"%.1f\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\" stroke=\"%s\" stroke-width=\"2.5\" stroke-dasharray=\"4\" marker-end=\"url(#arrow)\" />\n",
                    x1 + (CHIP_SIZE/2.0), y1, x1 + (CHIP_SIZE/2.0) + 20, y1, color);
            return;
        }

        if (to_idx >= (UINT)state.size() || pool[to_idx] == nullptr || state[to_idx].x == POS_INVALID) return;

        Pos p2 = state[to_idx];
        double x2 = p2.x * CELL_SIZE + CELL_SIZE / 2.0;
        double y2 = p2.y * CELL_SIZE + CELL_SIZE / 2.0;

        double dx = x2 - x1;
        double dy = y2 - y1;
        double dist = sqrt(dx*dx + dy*dy);
        if (dist < 0.1) return;

        // チップの縁で止まるように調整
        double ratio = (CHIP_SIZE / 2.0) / dist;
        fprintf(fp, "  <line x1=\"%.1f\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\" stroke=\"%s\" stroke-width=\"2.5\" marker-end=\"url(#arrow)\" />\n",
                x1 + dx * ratio, y1 + dy * ratio, x2 - dx * ratio, y2 - dy * ratio, color);
    };

    for (int i = 0; i < (int)state.size(); ++i) {
        if (pool[i] == nullptr) continue;
        draw_edge(i, pool[i]->m_next_g, "#28a745");
        if (pool[i]->valid_r()) draw_edge(i, pool[i]->m_next_r, "#dc3545");
    }

    fprintf(fp, "</svg>\n");
    fclose(fp);
}

//////////////////////////////////////////////////////////////////////////////

void chip_main(void){
	for(int i = 0; i < 10; ++i){
		IF(enemy_num(0, 416, 160, OKE_ALL))
			IF(enemy_num(0, 128, 320, OKE_HOVER) && friendly_num(0, 128, 320, OKE_ALL))
				turn_right();
				jump_backward();
			ELSE
				move_forward();
			ENDIF
			action1();
		ELSE
		ENDIF
		guard();
		fire(0, 416, 160, OKE_HOVER, 1, 8);
		fire(128, 20, 1, 8);
		option(1);
	}
}

//////////////////////////////////////////////////////////////////////////////
int main(void){

	CChipTree tree;
	g_pCurTree = &tree;

	try{
		chip_main();
	}catch(const OkeccError& e){
		puts(e.what());
		exit(0);
	}

	g_pCurTree->set_g(IDX_EXIT);

	// Goto 飛び先解決
	std::vector<UINT> IdxOld2New;
	std::vector<UINT> IdxNew2Old;

	for(UINT u = 0; u < g_ChipPool.m_list.size(); ++u){
		if(g_ChipPool[u]->m_id.get() != CHIPID_GOTO){
			g_ChipPool[u]->m_next_r = g_ChipPool.GetFinalDst(g_ChipPool[u]->m_next_r);
			g_ChipPool[u]->m_next_g = g_ChipPool.GetFinalDst(g_ChipPool[u]->m_next_g);
			IdxNew2Old.push_back(u);
		}else{
			delete g_ChipPool[u];
		}
		IdxOld2New.push_back((UINT)IdxNew2Old.size() - 1);
	}

	//Goto 削除
	for(UINT u = 0; u < IdxNew2Old.size(); ++u){
		g_ChipPool[u] = g_ChipPool[IdxNew2Old[u]];
		if(g_ChipPool[u]->valid_g()) g_ChipPool[u]->m_next_g = IdxOld2New[g_ChipPool[u]->m_next_g];
		if(g_ChipPool[u]->valid_r()) g_ChipPool[u]->m_next_r = IdxOld2New[g_ChipPool[u]->m_next_r];
	}
	g_ChipPool.m_start = IdxOld2New[tree.m_start];
	g_ChipPool.m_list.resize(IdxNew2Old.size());

	printf("Number of chip(s): %d\n", (UINT)g_ChipPool.m_list.size());
	for(UINT u = 0; u < g_ChipPool.m_list.size(); ++u){
		if(g_ChipPool[u]){
			printf("ID:%d: type:%d g:%d r:%d\n",
				u,
				g_ChipPool[u]->m_id.get(),
				g_ChipPool[u]->m_next_g,
				g_ChipPool[u]->m_next_r
			);
		}
	}
	//exit(0);

	CarnageSA sa(g_ChipPool);
	sa.run();
	sa.print_layout_svg("chip.svg");

	return 0;
}
