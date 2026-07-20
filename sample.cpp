// てきとうな手動操作 OKE

// 操作方法
// 移動方向はアナログスティック
#define BT_FIGHT	BT_CIRCLE	// ◯ 格闘
#define BT_JUMP		BT_CROSS	// ✕ ジャンプ
#define BT_FIRE		BT_R		// R 武装1 発射
#define BT_FIRE2	BT_L		// L 武装2 発射

#include "okecc.h"

void chip_main(){
	
	// 移動
	#define anX	A
	#define anY	B
	#define bt	C
	
	getButton;
	anX = analogX;
	anY = analogY;
	
	#define AN_TH	90
	
	If(isButton(BT_FIGHT))
		fight;
		Return;
	End
	
	If(isButton(BT_FIRE))
		fire(1, 3);
	End
	
	If(isButton(BT_FIRE2))
		fire(2, 1);
	End
	
	If(anY < -AN_TH)
		If(isButton(BT_JUMP))
			jumpForward;
		Else
			moveForward;
		End
	ElseIf(anY > AN_TH)
		If(isButton(BT_JUMP))
			jumpBackward;
		Else
			moveBackward;
		End;
	Else
		stopMove;
	End
	
	If(anX < - AN_TH)
		If(isButton(BT_JUMP))
			stopTurn;
			jumpLeft;
		Else
			turnLeft;
		End
	ElseIf(anX > AN_TH)
		If(isButton(BT_JUMP))
			stopTurn;
			jumpRight;
		Else
			turnRight;
		End
	Else
		stopTurn;
	End
	
	nop;
	
	If(anX > -AN_TH && anX < AN_TH && anY > -AN_TH && anY < AN_TH && isButton(BT_JUMP))
		jumpUp;
	End
}
