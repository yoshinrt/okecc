#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <algorithm>
#include <iterator>

class MemoryCardManager {
private:
	std::string filePath;
	const char *targetGameId;
	std::vector<uint8_t> cardData; // 128KBのメモリーカード全体を保持

	static const int MC_SIZE = 128 * 1024;   // 131,072 バイト
	static const int BLOCK_SIZE = 8192;	  // 8KB
	static const int DIR_FRAME_SIZE = 128;   // ディレクトリ情報の1単位

public:
	/**
	 * コンストラクタ: 指定されたファイルをメモリに読み込む
	 * @param path メモリーカードファイルのパス (.mcd / .mcr)
	 * @param gameId ターゲットとするゲームID (例: "SLPS-01234")
	 */
	explicit MemoryCardManager(const std::string& path, const char *gameId = nullptr) 
		: filePath(path), targetGameId(gameId) 
	{
		std::ifstream file(path, std::ios::binary);
		if (file) {
			cardData.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
		}
		
		// ファイルが存在しない、またはサイズが不正な場合は128KBで初期化
		if (cardData.size() < MC_SIZE) {
			cardData.resize(MC_SIZE, 0x00);
		}
	}
	
	void setGameId(const char *gameId){
		targetGameId = gameId;
	}
	
	/**
	 * 指定されたゲームIDのセーブデータを抽出し、その合計サイズを返す
	 * @param outData 抽出したデータを格納する配列
	 * @return データの合計バイト数 (見つからない場合は0)
	 */
	size_t read(std::vector<uint8_t>& outData) const {
		outData.clear();
		int currentBlock = findStartBlock();
		
		if (currentBlock == -1) {
			return 0; // ゲームIDが見つからない
		}

		// ブロックチェーンを辿ってデータを結合
		while (currentBlock >= 1 && currentBlock <= 15) {
			size_t offset = currentBlock * BLOCK_SIZE;
			
			// cardDataから8KB分をoutDataへ追加
			outData.insert(outData.end(), 
						   cardData.begin() + offset, 
						   cardData.begin() + offset + BLOCK_SIZE);

			// 次のブロック番号を取得 (ディレクトリフレームのオフセット0x08)
			uint16_t next = getNextBlockPointer(currentBlock);
			if (next == 0xFFFF) break; // 終端
			
			currentBlock = next + 1; // 内部ID(0-14)をブロック番号(1-15)へ変換
		}

		return outData.size();
	}

	/**
	 * メモリ配列の内容を、クラス内部の該当するゲーム領域に書き戻す
	 * @param inData 書き込むデータ (BLOCK_SIZEの倍数であること)
	 * @return 成功ならtrue
	 */
	bool write(const std::vector<uint8_t>& inData) {
		if (inData.empty() || inData.size() % BLOCK_SIZE != 0) return false;

		int currentBlock = findStartBlock();
		if (currentBlock == -1) return false;

		size_t dataOffset = 0;
		while (currentBlock >= 1 && currentBlock <= 15 && dataOffset < inData.size()) {
			size_t mcOffset = currentBlock * BLOCK_SIZE;
			
			// クラスメンバ(cardData)の該当箇所を上書き
			std::copy(inData.begin() + dataOffset, 
					  inData.begin() + dataOffset + BLOCK_SIZE, 
					  cardData.begin() + mcOffset);
			
			dataOffset += BLOCK_SIZE;
			uint16_t next = getNextBlockPointer(currentBlock);
			if (next == 0xFFFF || dataOffset >= inData.size()) break;
			currentBlock = next + 1;
		}
		return true;
	}

	/**
	 * クラス内部の全データをファイルへ書き出す
	 * @return 成功ならtrue
	 */
	bool saveToFile() const {
		std::ofstream file(filePath, std::ios::binary);
		if (!file) return false;
		file.write(reinterpret_cast<const char*>(cardData.data()), cardData.size());
		return file.good();
	}

private:
	// ゲームIDから開始ブロック番号(1-15)を特定する内部関数
	int findStartBlock() const {
		for (int i = 1; i < 16; ++i) {
			size_t dirOffset = i * DIR_FRAME_SIZE;
			// 0x51: セーブデータ開始フラグ
			if (cardData[dirOffset] == 0x51) {
				// オフセット12から最大20文字のIDを確認
				std::string idInCard(reinterpret_cast<const char*>(&cardData[dirOffset + 12]), 20);
				if (idInCard.find(targetGameId) != std::string::npos) {
					return i;
				}
			}
		}
		return -1;
	}

	// ディレクトリフレームから次のブロックへのリンク(0-14 or 0xFFFF)を取得
	uint16_t getNextBlockPointer(int blockIndex) const {
		size_t ptrOffset = blockIndex * DIR_FRAME_SIZE + 0x08;
		// リトルエンディアンとして読み取り
		return cardData[ptrOffset] | (cardData[ptrOffset + 1] << 8);
	}
};

typedef struct {
	uint8_t	dmy0000[0x400];
	
	struct Oke_t {
		uint8_t	OkeReadyStatus;
		uint8_t	dmy0401[0x1A4 - 1];
		
		struct {
			uint32_t StartSub2Y	: 4;
			uint32_t StartSub2X	: 5;
			uint32_t StartSub1Y	: 4;
			uint32_t StartSub1X	: 5;
			uint32_t StartMainY	: 4;
			uint32_t StartMainX	: 5;
		};
		
		uint32_t	Software[23 * 15];
	} Oke[25];
	
	uint8_t dmyB42C[0xDFFF - 0xB42C];
	
	uint8_t ChkSum;
} SaveDataZeus;

#if 0
// --- 使用例 ---

int main() {
	const std::string mcFile = "memcard1.mcd";
	const std::string gameId = "SLPS-01666";

	// 1. インスタンス生成（ファイル読み込みとID設定）
	MemoryCardManager manager(mcFile, gameId);

	// 2. データの読み出し
	std::vector<uint8_t> saveBuffer;
	size_t dataSize = manager.read(saveBuffer);

	if (dataSize > 0) {
		std::cout << "Game ID: " << gameId << " detected" << std::endl;
		std::cout << "Data size: " << dataSize << "Byte(s) (" << (dataSize / 1024) << " KB)" << std::endl;
		
		SaveDataZeus *p = (SaveDataZeus *)saveBuffer.data();
		
		for(int i = 0; i < 25; ++i){
			printf("%2d: %02X %2d %2d %2d %2d %2d %2d\n",
				i,
				p->Oke[i].OkeReadyStatus,
				p->Oke[i].StartMainX,
				p->Oke[i].StartMainY,
				p->Oke[i].StartSub1X,
				p->Oke[i].StartSub1Y,
				p->Oke[i].StartSub2X,
				p->Oke[i].StartSub2Y
			);
		}
		
		uint8_t ChkSum = 0;
		for(int i = 0; i < dataSize - 1; ++i){
			ChkSum += saveBuffer[i];
		}
		printf("%X\n", ChkSum);

	} else {
		std::cerr << "Not found Game ID '" << gameId << std::endl;
	}

	return 0;
}
#endif
