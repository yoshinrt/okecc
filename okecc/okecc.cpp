#include <iostream>
#include <string>
#include <fstream>
#include <peglib.h>
#include <wchar.h>
#include <format>

#define NO_OKECC_SYNTAX
#include "okecc.h"

//////////////////////////////////////////////////////////////////////////////

void traverse_ast(const std::shared_ptr<peg::Ast>& node, int depth = 0){
	if(!node) return;

	// インデント
	for(int i = 0; i < depth; ++i) std::cout << "  ";

	// ノード情報
	std::cout << node->name << "(" << node->choice << ")";

	if(!node->token.empty()){
		std::cout << " : \"" << node->token << "\"";
	}

	std::cout << std::endl;

	// 子ノードを再帰処理
	for(auto& child : node->nodes){
		traverse_ast(child, depth + 1);
	}
}

//////////////////////////////////////////////////////////////////////////////
// エラー処理

enum {
	E_Syntax,
};

void Error(std::string msg, const std::shared_ptr<peg::Ast>& node){
	std::cout << msg << " at line " << node->line << ", column " << node->column << std::endl;
}

void Error(int error_code, const std::shared_ptr<peg::Ast>& node){
	std::string msg;
	switch(error_code){
		case E_Syntax:
			msg = "Syntax Error";
			break;
	}

	Error(msg, node);
}

//////////////////////////////////////////////////////////////////////////////
// AST から関数情報を生成する

enum RetStatus {
	R_OK,
	R_Error,
};

class FunctionInfo {
public:
	enum Modifier {
		None,	// 仮想 sub
		Sub1,
		Sub2,
		Main,	// main
	};

	std::string name;
	std::vector<std::string> parameters;
	int modifier = None;
	std::shared_ptr<peg::Ast> node;
};

using FunctionInfoList = std::vector<std::unique_ptr<FunctionInfo>>;

FunctionInfoList GenFunctionList(const std::shared_ptr<peg::Ast>& node){
	using namespace peg::udl;

	FunctionInfoList func_info_list;

	if(node->tag != "Program"_){
		Error(E_Syntax, node);
		func_info_list.clear();
		return func_info_list;
	}

	for(auto& func_node : node->nodes){
		if(func_node->tag != "FunctionDefinition"_){
			func_info_list.clear();
			return func_info_list;
		}

		func_info_list.push_back(std::make_unique<FunctionInfo>());
		FunctionInfo& func_info = *func_info_list.back();

		// FunctionDefinition の処理
		for(auto& child : func_node->nodes){
			if(child->tag == "Identifier"_){
				// 関数名の処理
				func_info.name = child->token;
				if(func_info.name == "main") func_info.modifier = FunctionInfo::Main;
			}else if(child->tag == "ParameterList"_){
				// パラメータリストの処理
				for(auto& param : child->nodes){
					if(param->tag == "Identifier"_){
						func_info.parameters.push_back(std::string(param->token));
					}else{
						Error(E_Syntax, param);
						func_info_list.clear();
						return func_info_list;
					}
				}
			}else if(child->tag == "FunctionModifier"_){
				// main だったらエラー
				if(func_info.modifier == FunctionInfo::Main){
					Error(std::format("function modifier is not allowed formain function: {}", func_info.name), child);
					func_info_list.clear();
					return func_info_list;
				}
				// 関数修飾子の処理
				if(child->token == "sub1") func_info.modifier = FunctionInfo::Sub1;
				else if(child->token == "sub2") func_info.modifier = FunctionInfo::Sub2;
				else{
					Error(std::format("unknown function modifier: {}", child->token), child);
					func_info_list.clear();
					return func_info_list;
				}
			}else{
				// 関数定義の本体
				func_info.node = child;
			}
		}
	}

	return func_info_list;
}

//////////////////////////////////////////////////////////////////////////////
// コード生成

RetStatus GenCode(const std::shared_ptr<peg::Ast>& node){
	// ここでコード生成を行う
	// 例: ASTノードを解析して、対応するコードを生成する
	// この関数は、実際のコード生成ロジックに応じて実装する必要があります

	// 仮の実装として、ASTノードの情報を出力する
	std::cout << "Generating code for node: " << node->tag << std::endl;

	// 子ノードがある場合は再帰的に処理
	for(auto& child : node->nodes){
		if(GenCode(child) != R_OK){
			return R_Error;
		}
	}

	return R_OK;
}

//////////////////////////////////////////////////////////////////////////////

int main(int argc, char **argv){
	// argv[1] を open して std::string にリード
	if(argc <= 1){
		std::cerr << "Usage: " << argv[0] << " <source_code>" << std::endl;
		return -1;
	}

	std::string source_code;
	{
		std::ifstream file(argv[1]);
		if(!file){
			std::cerr << "Error opening file: " << argv[1] << std::endl;
			return -1;
		}
		source_code.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	}

	// 1. PEG文法の定義
	std::string grammar = R"(
		# 全体構造
		Program		<- FunctionDefinition + {no_ast_opt}

		# 関数定義 (3つのバリエーション)
		FunctionDefinition <- 'def' Identifier '(' ParameterList ')' FunctionModifier? CompoundStatement {no_ast_opt}
		FunctionModifier   <- 'sub1' / 'sub2'
		ParameterList	<- (Identifier ( ',' Identifier)*)?

		# 文 (Statements)
		Statement		<- CompoundStatement
						/ SelectionStatement
						/ IterationStatement
						/ JumpStatement
						/ ExpressionStatement

		CompoundStatement  <- '{' StatementList? '}'
		StatementList	<- Statement+ {no_ast_opt}

		ExpressionStatement <- Expression ';'

		SelectionStatement <- 'if' '(' Expression ')' Statement ( 'else' Statement)?
		IterationStatement <- 'while' '(' Expression ')' Statement
						/ 'do' Statement 'while' '(' Expression ')' ';'

		# return は値を返さない
		JumpStatement	<- 'break' ';'
						/ 'continue' ';'
						/ 'return' ';'

		# 式と演算子（C言語の優先順位通り、下に行くほど高優先順位）
		Expression		<- AssignmentExpression

		# 1. 代入演算子 (右結合のシミュレーション)
		AssignmentExpression <- (UnaryExpression AssignmentOperator AssignmentExpression) / LogicalOrExpression
		AssignmentOperator   <- '+=' / '-=' / '*=' / '/=' / '%=' / '='

		# 2. 論理和 (||)
		LogicalOrExpression  <- LogicalAndExpression ( '||' LogicalAndExpression)*

		# 3. 論理積 (&&)
		LogicalAndExpression <- EqualityExpression ( '&&' EqualityExpression)*

		# 4. 等価比較 (==, !=)
		EqualityExpression   <- RelationalExpression ( EqualityOperator RelationalExpression)*
		EqualityOperator	<- '==' / '!='

		# 5. 大小比較 (<, <=, >, >=)
		RelationalExpression <- AdditiveExpression ( RelationalOperator AdditiveExpression)*
		RelationalOperator   <- '<=' / '>=' / '<' / '>'

		# 6. 加減算 (+, -)
		AdditiveExpression   <- MultiplicativeExpression ( AdditiveOperator MultiplicativeExpression)*
		AdditiveOperator	<- '+' / '-'

		# 7. 乗除・剰余算 (*, /, %)
		MultiplicativeExpression <- UnaryExpression ( MultiplicativeOperator UnaryExpression)*
		MultiplicativeOperator   <- '*' / '/' / '%'

		# 8. 単項演算子・前置演算子 (!, +, -, 前置++, 前置--)
		UnaryExpression	<- PrefixOperator UnaryExpression
							/ PrimaryExpression
		PrefixOperator	<- '++' / '--' / '!' / '+' / '-'

#		# 9. 後置演算子 (後置++, 後置--)
#		PostfixExpression	<- PrimaryExpression ( PostfixOperator)*
#		PostfixOperator	<- '++' / '--'

		# 10. 最優先要素（関数呼び出し、識別子、定数、括弧）
		PrimaryExpression	<- FunctionCall
							/ Identifier
							/ Constant
							/ '(' Expression ')'

		# 関数呼び出し「識別子(...)」のみ
		FunctionCall		<- Identifier '(' ArgumentExpressionList? ')' {no_ast_opt}
		ArgumentExpressionList <- AssignmentExpression ( ',' AssignmentExpression)*

		# 定数 (16進数、10進数)
		Constant			<- HexadecimalConstant / DecimalConstant
		HexadecimalConstant  <- < '0' [xX] [0-9a-fA-F]+ >
		DecimalConstant	<- < [0-9]+ >

		# 識別子 (C言語準拠)
		Identifier		<- < [a-zA-Z_] [a-zA-Z0-9_]* >

		#%whitespace <- [ \t\r\n]*
		%word       <- [a-zA-Z_] [a-zA-Z0-9_]*
		%whitespace <- ([ \t\r\n] / '//' [^\n]*)*
	)";

	// 2. パーサーの生成とセットアップ
	peg::parser parser;

	// 最新のログ出力設定
	parser.set_logger([](size_t ln, size_t col, const std::string& msg){
		std::cerr << ln << ":" << col << ": " << msg << std::endl;
	});

	// 文法の読み込み
	if(!parser.load_grammar(grammar.c_str(), grammar.size())){
		std::cerr << "Grammar error!" << std::endl;
		return -1;
	}

	// 最新の自動AST生成を有効化
	parser.enable_ast();
	parser.enable_packrat_parsing();
	//peg::enable_tracing(parser, std::cout);

	// 4. パースの実行
	std::shared_ptr<peg::Ast> ast;
	if(!parser.parse(source_code, ast)){
		std::cerr << "Parse Failed." << std::endl;
		return -1;
	}

	// 不要な中間ノードをスマートに間引く（最適化）
	ast = parser.optimize_ast(ast);

	// 最新の文字列表現へ変換して出力
	//std::cout << peg::ast_to_s(ast) << std::endl;
	traverse_ast(ast, 0);
	auto func_info_list = GenFunctionList(ast);

	if(func_info_list.empty()){
		return -1;
	}

	// main, sub1, sub2 の関数を探す
	FunctionInfo* vital_func[3] = { nullptr, nullptr, nullptr };

	for (auto& func_info_ptr : func_info_list){
		FunctionInfo* func_info = func_info_ptr.get();
		if(func_info->modifier == FunctionInfo::Main){
			vital_func[0] = func_info;
		}else if(func_info->modifier == FunctionInfo::Sub1){
			vital_func[1] = func_info;
		}else if(func_info->modifier == FunctionInfo::Sub2){
			vital_func[2] = func_info;
		}
	}

	// main が存在しない場合はエラー
	if (!vital_func[0]) {
		std::cout << "Error: main function not found." << std::endl;
		return -1;
	}

	// コード生成
	g_pField.push_back(std::make_unique<CField>("MAIN", 15, 15));
	g_pField.push_back(std::make_unique<CField>("SUB1", 7, 7));
	g_pField.push_back(std::make_unique<CField>("SUB2", 7, 7));
	g_pCurField = g_pField[0].get();

	for(int i = 0; i < 3; ++i){
		if(vital_func[i]){
			g_pCurField = g_pField[i].get();
			if(GenCode(vital_func[i]->node) != R_OK) break;
		}
	}

	return 0;
}
