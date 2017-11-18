#include <iostream>
#include <fstream>
#include <queue>

const int R = 256;

struct Node {
public:
    unsigned char ch;
    int freq;
    Node *left, *right;

    Node() = default;

    Node(unsigned char character, int frequency, Node *leftNode, Node *rightNode) :
            ch(character), freq(frequency), left(leftNode), right(rightNode) {}

    bool isLeaf() {
        return (this->left == nullptr) && (this->right == nullptr);
    }
};

struct NodeCompare {
    bool operator()(const Node *t1, const Node *t2) const {
        return t1->freq > t2->freq;
    }
};

class BitWriter {
public:
    explicit BitWriter(std::ofstream &output) : buffer('\0'), n(0), os(output) {}

    void writeBit(bool bit) {
        this->buffer <<= 1;
        if (bit) this->buffer |= 1;
        this->n++;
        if (this->n == 8) this->clearBuffer();
    }

    void writeByte(unsigned char x) {
        if (this->n == 0) this->os.write(reinterpret_cast<char *>(&x), sizeof(unsigned char));
        else {
            for (int i = 0; i < 8; ++i) {
                bool bit = ((x >> (8 - i - 1)) & 1) == 1;
                this->writeBit(bit);
            }
        }
    }

    void writeUnsigned(unsigned int x) {
        this->writeByte(static_cast<unsigned char>((x >> 24) & 0xff));
        this->writeByte(static_cast<unsigned char>((x >> 16) & 0xff));
        this->writeByte(static_cast<unsigned char>((x >> 8) & 0xff));
        this->writeByte(static_cast<unsigned char>((x >> 0) & 0xff));
    }

    void clearBuffer() {
        if (this->n == 0) return;
        if (this->n > 0) this->buffer <<= (8 - n);
        this->os.write(reinterpret_cast<char *>(&buffer), sizeof(unsigned char));
        this->buffer = 0;
        this->n = 0;
    }

    void close() {
        this->clearBuffer();
        this->os.close();
    }

private:
    unsigned char buffer;
    int n;
    std::ofstream &os;
};

class BitReader {
public:
    explicit BitReader(std::ifstream &input) : buffer(0), n(0), is(input) { fillBuffer(); }

    void fillBuffer() {
        if (!this->is.eof()) {
            this->is.read(reinterpret_cast<char *>(&buffer), sizeof(unsigned char));
            this->n = 8;
        } else {
            this->buffer = static_cast<unsigned char>(-1);
            this->n = -1;
        }
    }

    bool readBit() {
        --this->n;
        bool bit = ((this->buffer >> this->n) & 1) == 1;
        if (this->n == 0) this->fillBuffer();
        return bit;
    }

    unsigned char readChar() {
        if (this->n == 8) {
            unsigned char x = this->buffer;
            this->fillBuffer();
            return x;
        }
        unsigned char x = this->buffer;
        x <<= (8 - n);
        int oldN = this->n;
        this->fillBuffer();
        this->n = oldN;
        x |= (this->buffer) >> this->n;
        return x;
    }

    unsigned int readUnsigned() {
        unsigned int x = 0;
        for (int i = 0; i < 4; ++i) {
            unsigned char c = this->readChar();
            x <<= 8;
            x |= c;
        }
        return x;
    }

    void close() {
        this->is.close();
    }

private:
    unsigned char buffer;
    int n;
    std::ifstream &is;
};

void buildCode(std::string huffmanCode[], Node *x, std::string s);

Node *buildTrie(int *freq);

void writeTire(Node *x, BitWriter &fileWriter);

Node *readTire(BitReader &fileReader);

void compress(const std::string &fileNameIn, const std::string &fileNameOut) {
    std::cout << "compressing..." << std::endl;
    std::ifstream fin(fileNameIn, std::ios::in | std::ios::binary);
    unsigned int fileLength = 0;
    unsigned char byte;
    int freq[R] = {0};
    while (fin.read(reinterpret_cast<char *>(&byte), sizeof(unsigned char))) {
        ++freq[byte];
        ++fileLength;
    }
    fin.close();
    std::string huffman[R] = {""};
    auto root = buildTrie(freq);
    buildCode(huffman, root, "");
    std::ofstream fout(fileNameOut, std::ios::out | std::ios::binary);
    BitWriter fileWriter(fout);
    writeTire(root, fileWriter);
    fileWriter.writeUnsigned(fileLength);
    fin.open(fileNameIn, std::ios::in | std::ios::binary);
    for (int i = 0; i < fileLength; ++i) {
        fin.read(reinterpret_cast<char *>(&byte), sizeof(unsigned char));
        std::string code = huffman[byte];
        for (auto ch : code) {
            if (ch == '0') fileWriter.writeBit(false);
            else fileWriter.writeBit(true);
        }
    }
    fin.close();
    fileWriter.close();
    std::cout << "compression completed" << std::endl;
}

void deCompress(const std::string &fileNameIn, const std::string &fileNameOut) {
    std::cout << "decompressing..." << std::endl;
    std::ifstream fin(fileNameIn, std::ios::in | std::ios::binary);
    std::ofstream fout(fileNameOut, std::ios::out | std::ios::binary);
    BitReader fileReader(fin);
    auto root = readTire(fileReader);
    unsigned int length = fileReader.readUnsigned();
    for (int i = 0; i < length; ++i) {
        auto x = root;
        while (!x->isLeaf()) {
            bool bit = fileReader.readBit();
            if (bit) x = x->right;
            else x = x->left;
        }
        fout.write(reinterpret_cast<char *>(&(x->ch)), sizeof(unsigned char));
    }
    fileReader.close();
    fout.close();
    std::cout << "decompression completed" << std::endl;
}

Node *buildTrie(int *freq) {
    std::priority_queue<Node *, std::vector<Node *>, NodeCompare> queue;
    Node *left, *right;
    for (int i = 0; i < R; ++i) {
        if (freq[i] > 0)
            queue.push(new Node(static_cast<unsigned char>(i), freq[i], nullptr, nullptr));
    }
    if (queue.size() == 1) {
        if (freq[0] == 0) queue.push(new Node('\0', 0, nullptr, nullptr));
        else queue.push(new Node('\1', 0, nullptr, nullptr));
    }

    while (queue.size() > 1) {
        left = queue.top();
        queue.pop();
        right = queue.top();
        queue.pop();
        queue.push(new Node('\0', left->freq + right->freq, left, right));
    }
    return queue.top();
}

void buildCode(std::string huffmanCode[], Node *x, std::string s) {
    if (!(x->isLeaf())) {
        buildCode(huffmanCode, x->left, s + '0');
        buildCode(huffmanCode, x->right, s + '1');
    } else {
        huffmanCode[x->ch] = s;
    }
}

void writeTire(Node *x, BitWriter &fileWriter) {
    if (x->isLeaf()) {
        fileWriter.writeBit(true);
        fileWriter.writeByte(x->ch);
        return;
    }
    fileWriter.writeBit(false);
    writeTire(x->left, fileWriter);
    writeTire(x->right, fileWriter);
}

Node *readTire(BitReader &fileReader) {
    bool isLeaf = fileReader.readBit();
    if (isLeaf) {
        return new Node(fileReader.readChar(), -1, nullptr, nullptr);
    } else {
        return new Node('\0', -1, readTire(fileReader), readTire(fileReader));
    }
}

int main(int argc, char *argv[]) {
    std::cout << "huffman file compressor by libertyeagle" << std::endl;
    if (strcmp(argv[1], "-z") == 0) compress(argv[2], argv[3]);
    else if (strcmp(argv[1], "-u") == 0) deCompress(argv[2], argv[3]);
    else if (strcmp(argv[1], "-h") == 0) {
        std::cout << "-z [source] [target] -> compress" << std::endl;
        std::cout << "-u [source] [target] -> decompress" << std::endl;
        std::cout << "-h -> help" << std::endl;
    }
    return 0;
}