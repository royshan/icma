/** \file CMA_ME_Knowledge.cc
 *
 * \brief The Knowledge class of the CMAC
 *
 * \author vernkin
 * \version 0.1
 * \date Feb 17, 2009
 */

#include <string>


#include "CMA_ME_Knowledge.h"
#include "strutil.h"
#include "pos_table.h"

#include <assert.h>
using namespace std;

namespace cma{

vector<string> ENCODING_VEC;

bool ENC_VEC_INIT_FLAG = false;

inline bool fileExists(const char* path)
{
	ifstream posIn(path);
	if(posIn)
	{
		posIn.close();
		return true;
	}
	return false;
}

CMA_ME_Knowledge::CMA_ME_Knowledge(): segT_(0), posT_(0),vsynC_(0),trie_(new VTrie()){
    CMA_ME_Knowledge::initialize();
}

CMA_ME_Knowledge::~CMA_ME_Knowledge(){
    delete segT_;
    delete posT_;
    delete vsynC_;
    delete trie_;
}

int CMA_ME_Knowledge::loadPOSModel(const char* cateName){
    string cateStr(cateName);

    //load pos table first
    ifstream posFile((cateStr + ".pos").data());
    assert(posFile);
    string line;
    while(!posFile.eof()){
        getline(posFile, line);
        trimSelf(line);
        if(line.empty())
            continue;
        POSTable::instance()->addPOS(line);
    }

    assert(!posT_);
    posT_ = new POSTagger((cateStr + ".model").data(), trie_);

    return 1;
}

int CMA_ME_Knowledge::loadStatModel(const char* cateName){
    assert(!segT_);

    string cateStr(cateName);
    segT_ = new SegTagger(cateStr, trie_);

    //try to load black words here
    string blackWordFile = cateStr.substr(0, cateStr.length() - 4) + ".black";
    ifstream bwIn(blackWordFile.data());
    if(bwIn)
	{
		string line;
		while(!bwIn.eof()){
			getline(bwIn, line);
			trimSelf(line);
			if(line.length() <= 0)
				continue;
			blackWords_.insert(line);
		}
		bwIn.close();
	}

    return 1;
}

int CMA_ME_Knowledge::loadSynonymDictionary(const char* fileName){
    assert(!vsynC_);
    vsynC_ = new VSynonymContainer(fileName, " ", "_");
    return 1;
}

void CMA_ME_Knowledge::getSynonyms(const string& word, VSynonym& synonym){
    vsynC_->get_synonyms(word, synonym);
}

int CMA_ME_Knowledge::loadStopWordDict(const char* fileName){
    ifstream in(fileName);
    if(!in)
    	return 0;
    string line;
    while(!in.eof()){
        getline(in, line);
        trimSelf(line);
        if(line.length() <= 0)
            continue;
        stopWords_.insert(line);
    }
    in.close();
    return 1;
}

int CMA_ME_Knowledge::loadSystemDict_(const char* binFileName){
	FILE *in = fopen(binFileName, "r");
    if(!in)
    	return 0;
    string line;
    while(!feof(in)){
        //may be another way get line
        line = readEncryptLine(in);
        if(line.empty())
            continue;
        appendWordPOS(line);
    }
    fclose(in);
    return 1;
}

int CMA_ME_Knowledge::loadSystemDict(const char* binFileName){
    if(!trie_)
        trie_ = new VTrie();

    int curRet = loadSystemDict_(binFileName);
    int ret = curRet;
    string destFile(binFileName);
    while(curRet)
    {
    	std::ostringstream buffer;
    	buffer << destFile << "." << ret;
    	curRet = loadSystemDict_(buffer.str().data());
    	ret += curRet;
    }

    return ret;
}

int CMA_ME_Knowledge::loadUserDict_(const char* fileName){
	ifstream in(fileName);
    if(!in)
    	return 0;
    cout<<"load user dic "<<fileName<<endl;
    string line;
    while(!in.eof()){
        getline(in, line);
        if(line.empty())
        	continue;
        appendWordPOS(line);
    }

    in.close();
    return 1;
}

int CMA_ME_Knowledge::loadUserDict(const char* fileName){
    if(!trie_)
        trie_ = new VTrie();

    int curRet = loadUserDict_(fileName);
    int ret = curRet;
    string destFile(fileName);
    while(curRet)
    {
    	std::ostringstream buffer;
    	buffer << destFile << "." << ret;
    	curRet = loadUserDict_(buffer.str().data());
    	ret += curRet;
    }

    return ret;
}


int CMA_ME_Knowledge::encodeSystemDict(const char* txtFileName,
        const char* binFileName){
    ifstream in(txtFileName);
    FILE *out = fopen(binFileName, "w");
    string line;

    int seCode[] = {0x12, 0x34, 0x54, 0x27};

    while(!in.eof()){
        getline(in, line);
        int size = (int)line.size();
        if(!size)
            continue;
        char buf[size + 1];
        buf[size] = '\0';
        strncpy(buf, line.data(), size);    

        for(int i=0; i<size; ++i){
            if( i % 2 == 0)
                buf[i] += 2;
            else if( i % 3 == 0)
                buf[i] += 3;
            else if( i % 5 == 0)
                buf[i] += 5;
            else if( i % 7 == 0)
                buf[i] += 7;
            buf[i] ^= seCode[ i % 4 ];
        }

        for(int i=0; i<=size/2; i+=2){
            char tmp = buf[i];
            buf[i] = buf[ size - 1 - i ];
            buf[ size - 1 - i ] = tmp;
        }

        //swap the length bit
        fputc(size >> 8 & 0xff, out);
        fputc(size >> 24 & 0xff, out);
        fputc(size & 0xff, out);
        fputc(size >> 16 & 0xff, out);
        fputs(buf, out);
    }

    in.close();
    fclose(out);
    return 1;
}

SegTagger* CMA_ME_Knowledge::getSegTagger() const{
    return segT_;
}

POSTagger* CMA_ME_Knowledge::getPOSTagger() const{
    return posT_;
}

string CMA_ME_Knowledge::readEncryptLine(FILE *in){
    int seCode[] = {0x12, 0x34, 0x54, 0x27};
    int lenBuf[4];
    lenBuf[0] = fgetc(in);
    if(lenBuf[0] == -1)
        return "";
    for(int i=1; i<4; ++i){
        lenBuf[i] = fgetc(in);
    }
    int size = (lenBuf[0] << 8) + (lenBuf[1] << 24) + (lenBuf[2]) + (lenBuf[3] << 16);

    char buf[size + 1];
    buf[size] = '\0';
    fgets(buf, size + 1, in);

    for(int i=0; i<=size/2; i+=2){
        char tmp = buf[i];
        buf[i] = buf[ size - 1 - i ];
        buf[ size - 1 - i ] = tmp;
    }

    for(int i=0; i<size; ++i){
        buf[i] ^= seCode[ i % 4 ];
        if( i % 2 == 0)
            buf[i] -= 2;
        else if( i % 3 == 0)
            buf[i] -= 3;
        else if( i % 5 == 0)
            buf[i] -= 5;
        else if( i % 7 == 0)
            buf[i] -= 7;
    }

    return string(buf);
}

bool CMA_ME_Knowledge::isStopWord(const string& word){
    return stopWords_.find(word) != stopWords_.end();
}

VTrie* CMA_ME_Knowledge::getTrie(){
    return trie_;
}

void CMA_ME_Knowledge::initialize(){
    if(ENC_VEC_INIT_FLAG)
        return;

    ENC_VEC_INIT_FLAG = true;
    ENCODING_VEC.resize(ENCODE_TYPE_NUM + 1);
    ENCODING_VEC[ENCODE_TYPE_GB2312] = "gb2312";
    ENCODING_VEC[ENCODE_TYPE_BIG5] = "big5";
    ENCODING_VEC[ENCODE_TYPE_GB18030] = "gb18030";
}

int CMA_ME_Knowledge::loadConfig(const char* fileName)
{
    POSTable* posTable = POSTable::instance();
    bool r = posTable->loadConfig(fileName);

    return r ? 1 : 0;
}

bool CMA_ME_Knowledge::appendWordPOS(string& line){
    vector<string> tokens;
    TOKEN_STR(line, tokens);
    size_t n = tokens.size();
    if(!n){
        return false;
    }
    string word = tokens[0];
    replaceAll(word, "_", " ");

    if(blackWords_.find(word) != blackWords_.end())
    {
    	return false;
    }
    set<string>* posSet = 0;
    //try to search first
    VTrieNode node;
    trie_->search(word.data(), &node);
    //already exits
    if(node.data > 0){
        if(posT_)
			posSet = &(posT_->posVec_[node.data]);
    }else{

    	if(posT_)
    	{
			//get the right offset (offset 0 is reserved)
			node.data = (int)posT_->posVec_.size();
			//insert new key
			posT_->posVec_.push_back(set<string>());
			posSet = &(posT_->posVec_.back());

			trie_->insert(word.data(), &node);
    	}
    	else
    	{
    		node.data = 1;
    		trie_->insert(word.data(), &node);
    	}
    }

    if(posT_)
    {
		for(size_t i=1; i<n; ++i){
			posSet->insert(tokens[i]);
		}
    }


    return true;
}

}
