/** \file CMA_ME_Analyzer.cc
 * \brief Analyzer for the CMAC
 *
 * \author vernkin
 *
 */

#include <string>
#include <fstream>
#include <iostream>

#include "VTrie.h"
#include "strutil.h"

#include "icma/pos_table.h"
#include "icma/me/CMA_ME_Analyzer.h"
#include "icma/util/CPPStringUtils.h"
#include "icma/util/CateStrTokenizer.h"
#include "icma/util/tokenizer.h"

#include "icma/fmincover/analysis_fmincover.h"

namespace cma {

/**
 * Whether the two MorphemeLists is the same
 * \param list1 the MorphemeList 1
 * \param list2 the MorphemeList 2
 */
inline bool isSameMorphemeList( const MorphemeList* list1, const MorphemeList* list2, bool printPOS )
{
	// if one is zero pointer, return null
	if( !list1 || !list2 )
	{
		return false;
	}

	if( list1->size() != list2->size() )
		return false;
	//compare one by one
	size_t N = list1->size();
	if( printPOS )
	{
		for( size_t i = 0; i < N; ++i )
		{
			const Morpheme& m1 = (*list1)[i];
			const Morpheme& m2 = (*list2)[i];
			if( m1.lexicon_ != m2.lexicon_ || m1.posCode_ != m2.posCode_)
			{
				return false;
			}
		}
	}
	else
	{
		for( size_t i = 0; i < N; ++i )
		{
			if( (*list1)[i].lexicon_ != (*list2)[i].lexicon_ )
				return false;
		}
	}
	//all the elements are the same
	return true;
}


    CMA_ME_Analyzer::CMA_ME_Analyzer()
			: knowledge_(0), ctype_(0), posTable_(0),
			  analysis(&CMA_ME_Analyzer::analysis_mmmodel)
    {
    }

    CMA_ME_Analyzer::~CMA_ME_Analyzer() {
    }

    void CMA_ME_Analyzer::setOption(OptionType nOption, double nValue)
    {
        // standard implementation
        // check nbest value range
        if(nOption == OPTION_TYPE_NBEST && nValue < 1)
        {
        return;
        }

        options_[nOption] = nValue;
        // check for specific setting
        if( nOption == OPTION_ANALYSIS_TYPE )
        {
            if( static_cast<int>(nValue) == 2 )
                analysis = &CMA_ME_Analyzer::analysis_fmm;
            else if( static_cast<int>(nValue) == 3 )
                analysis = &CMA_ME_Analyzer::analysis_fmincover;
            else
                analysis = &CMA_ME_Analyzer::analysis_mmmodel;
        }
    }

    int CMA_ME_Analyzer::runWithSentence(Sentence& sentence) {
        if( strlen( sentence.getString() ) == 0 )
        	return 1;
        int N = (int) getOption(OPTION_TYPE_NBEST);
        bool printPOS = getOption(OPTION_TYPE_POS_TAGGING) > 0;
        
        vector<pair<vector<string>, double> > segment;
        vector<vector<string> > pos;

        (this->*analysis)(sentence.getString(), N, pos, segment, printPOS);

        if(N <= 1)
        {
        	MorphemeList list;
			vector<string>& segs = segment[0].first;
			vector<string>& poses = pos[0];
			size_t segSize = segs.size();
			for (size_t j = 0; j < segSize; ++j) {
				string& seg = segs[j];
				if(knowledge_->isStopWord(seg))
					continue;
				Morpheme morp;
				morp.lexicon_ = seg;
				if(printPOS)
				{
					morp.posStr_ = poses[j];
					morp.posCode_ = posTable_->getCodeFromStr(morp.posStr_);
					morp.isIndexed = posTable_->isIndexPOS( morp.posCode_ );
				}
				list.push_back(morp);
			}
			sentence.addList(list, 1.0);
			return 1;
        }

        size_t size = segment.size();
        double totalScore = 0;

		for (size_t i = 0; i < size; ++i) {
			MorphemeList list;
			vector<string>& segs = segment[i].first;
			vector<string>& poses = pos[i];
			size_t segSize = segs.size();
			for (size_t j = 0; j < segSize; ++j) {
				string& seg = segs[j];
				if(knowledge_->isStopWord(seg))
					continue;
				Morpheme morp;
				morp.lexicon_ = seg;
				if(printPOS)
				{
					morp.posStr_ = poses[j];
					morp.posCode_ = posTable_->getCodeFromStr(morp.posStr_);
					morp.isIndexed = posTable_->isIndexPOS( morp.posCode_ );
				}

				list.push_back(morp);
			}

			bool isDupl = false;

			//check the current result with exits results
			for( int listOffset = sentence.getListSize() - 1 ; listOffset >= 0; --listOffset )
			{
				if( isSameMorphemeList( sentence.getMorphemeList(listOffset), &list, printPOS ) )
				{
					isDupl = true;
					break;
				}
			}
			//ignore the duplicate results
			if( isDupl )
				continue;

			double score = segment[i].second;
			totalScore += score;
			sentence.addList( list, score );
		}

		for ( int j = 0; j < sentence.getListSize(); ++j )
		{
			sentence.setScore(j, sentence.getScore(j) / totalScore );
		}

        return 1;
    }

    int CMA_ME_Analyzer::runWithStream(const char* inFileName, const char* outFileName) {

    	assert(inFileName);
    	assert(outFileName);

    	ifstream in(inFileName);
		if(!in)
		{
			cerr<<"[Error] The input file "<<inFileName<<" not exists!"<<endl;
			return 0;
		}

		ofstream out(outFileName);
		if(!out)
		{
			cerr<<"[Error] The output file "<<outFileName<<" could not be created!"<<endl;
			return 0;
		}

    	int N = 1;
        bool printPOS = getOption(OPTION_TYPE_POS_TAGGING) > 0;

        string line;
        bool remains = !in.eof();
        while (remains) {
            getline(in, line);
            remains = !in.eof();
            if (!line.length()) {
                if( remains )
					out << endl;
                continue;
            }
            vector<pair<vector<string>, double> > segment;
            vector<vector<string> > pos;
            (this->*analysis)(line.data(), N, pos, segment, printPOS);

            if (printPOS)
            {
                vector<string>& best = segment[0].first;
                vector<string>& bestPOS = pos[0];
                int maxIndex = (int)best.size();
                for (int i = 0; i < maxIndex; ++i) {
                    if(knowledge_->isStopWord(best[i]))
                    	continue;
                	out << best[i] << posDelimiter_ << bestPOS[i] << wordDelimiter_;
                }

                if (remains)
                    out << sentenceDelimiter_ << endl;
                else
                    break;
            }
            else
            {
                vector<string>& best = segment[0].first;
                int maxIndex = (int)best.size();
                for (int i = 0; i < maxIndex; ++i) {
                	if(knowledge_->isStopWord(best[i]))
                		continue;
                	out << best[i] << wordDelimiter_;
                }

                if (remains)
                	out << sentenceDelimiter_ << endl;
                else
                    break;
            }
        }

        in.close();
        out.close();

        return 1;
    }

    const char* CMA_ME_Analyzer::runWithString(const char* inStr) {
    	strBuf_.clear();

    	if( strlen( inStr ) == 0 )
    	      return strBuf_.c_str();

    	bool printPOS = getOption(OPTION_TYPE_POS_TAGGING) > 0;
      
        vector<pair<vector<string>, double> > segment;
        vector<vector<string> > pos;
        (this->*analysis)(inStr, 1, pos, segment, printPOS);

        if (printPOS)
        {
            vector<string>& best = segment.begin()->first;
            vector<string>& bestPOS = pos[0];
            size_t size = best.size();
            for (size_t i = 0; i < size; ++i) {
                if(knowledge_->isStopWord(best[i]))
                	continue;
            	strBuf_.append(best[i]).append(posDelimiter_).
                      append(bestPOS[i]).append(wordDelimiter_);
            }
        }
        else
        {
            vector<string>& best = segment[0].first;
            size_t size = best.size();
            for (size_t i = 0; i < size; ++i) {
            	if(knowledge_->isStopWord(best[i]))
            		continue;
            	strBuf_.append(best[i]).append(wordDelimiter_);
            }
        }

        return strBuf_.c_str();
    }

    void CMA_ME_Analyzer::getNGramResult( const char *inStr, int n, vector<string>& output )
    {
    	vector<vector<OneGramType> > oneGram;
    	splitToOneGram( inStr, oneGram );
    	getNGramResultImpl( oneGram, n, output );
    }

    void CMA_ME_Analyzer::getNGramArrayResult( const char *inStr, vector<int> nArray, vector<string>& output )
	{
    	vector<vector<OneGramType> > oneGram;
		splitToOneGram( inStr, oneGram );
		for( vector<int>::iterator itr = nArray.begin();
				itr != nArray.end(); ++itr)
			getNGramResultImpl( oneGram, *itr, output );
	}

    void CMA_ME_Analyzer::setKnowledge(Knowledge* pKnowledge) {
        knowledge_ = (CMA_ME_Knowledge*) pKnowledge;
        // close the POS output automatically
        if(!knowledge_->isSupportPOS())
        	setOption(Analyzer::OPTION_TYPE_POS_TAGGING, 0);
        posTable_ = knowledge_->getPOSTable();
        ctype_ = CMA_CType::instance(knowledge_->getEncodeType());
        encodeType_ = knowledge_->getEncodeType();
        if(knowledge_->getPOSTagger())
            knowledge_->getPOSTagger()->setCType(ctype_);
        if(knowledge_->getSegTagger())
            knowledge_->getSegTagger()->setCType(ctype_);

        const string* val = knowledge_->getSystemProperty( "pos_delimiter" );
        if( val )
        	setPOSDelimiter( val->data() );

        val = knowledge_->getSystemProperty( "word_delimiter" );
		if( val )
			setWordDelimiter( val->data() );

		val = knowledge_->getSystemProperty( "sentence_delimiter" );
		if( val )
			setSentenceDelimiter( val->data() );

    }

    void CMA_ME_Analyzer::splitSentence(const char* paragraph, std::vector<Sentence>& sentences)
    {
        if(! paragraph)
            return;

        Sentence result;
        string sentenceStr;
        CTypeTokenizer tokenizer(ctype_);
        tokenizer.assign(paragraph);
        for(const char* p=tokenizer.next(); p; p=tokenizer.next())
        {
            if(ctype_->isSentenceSeparator(p))
            {
                sentenceStr += p;

                result.setString(sentenceStr.c_str());
                sentences.push_back(result);

                sentenceStr.clear();
            }
            // white-space characters are also used as sentence separator,
            // but they are ignored in the sentence result
            else if(ctype_->isSpace(p))
            {
                if(! sentenceStr.empty())
                {
                    result.setString(sentenceStr.c_str());
                    sentences.push_back(result);

                    sentenceStr.clear();
                }
            }
            else
            {
                sentenceStr += p;
            }
        }

        // in case the last character is not space or sentence separator
        if(! sentenceStr.empty())
        {
            result.setString(sentenceStr.c_str());
            sentences.push_back(result);

            sentenceStr.clear();
        }
    }

namespace meanainner{

    /**
     * \param lastWordEnd include that index
     */
    inline void toCombine(VTrie *trie, CMA_CType* type, vector<string>& src,
            int begin, int lastWordEnd, vector<string>& dest){
        if(begin == lastWordEnd){
            string& str = src[begin];
            if(!type->isSpace(str.c_str()))
                dest.push_back(str);
            return;
        }
        bool isOK = true;
        //only have two word
        /*if((lastWordEnd - begin < 2) &&
                (type->length(src[begin].data()) + type->length(src[begin+1].data()) < 3)){
            VTrieNode node;
            trie->search(src[begin].data(), &node);
            if(node.data > 0){
                trie->search(src[begin + 1].data(), &node);
                isOK = node.data <= 0;
            }
        }*/

        if(isOK){
            string buf = src[begin];
            for(int k=begin+1; k<=lastWordEnd; ++k)
                buf.append(src[k]);
            dest.push_back(buf);
        }else{
            for(int k=begin; k<=lastWordEnd; ++k){
                string& str = src[k];
                if(!type->isSpace(str.c_str()))
                    dest.push_back(str);
            }
        }
    }

    void combineRetWithTrie(VTrie *trie, vector<string>& src, 
            vector<string>& dest, CMA_CType* type) {
        
        int begin = -1;
        int lastWordEnd = -1;

        int n = (int)src.size();
        VTrieNode node;
        for (int i = 0; i < n; ++i) {
            string& str = src[i];

            #ifdef DEBUG_TRIE_MATCH
            cout<<"Before Check begin:"<<begin<<node<<endl;
            #endif

            size_t strLen = str.length();
            size_t j = 0;
            for (; node.moreLong && j < strLen; ++j) {
                trie->find(str[j], &node);
                //cout<<"Ret "<<(int)(unsigned char)str[j]<<","<<node<<endl;
            }

            #ifdef DEBUG_TRIE_MATCH
            cout<<"Check str "<<str<<",isEnd:"<<(j == strLen)<<node<<endl;
            #endif

            //did not reach the last bit
            if (j < strLen) {
                //no exist in the dictionary
                if (begin < 0)
                {
                    if(!type->isSpace(str.c_str()))
                        dest.push_back(str);
                }
                 else {
                    if(lastWordEnd < begin)
                        lastWordEnd = begin;
                    toCombine(trie, type, src, begin, lastWordEnd, dest);
                    begin = -1;
                    //restart that node
                    i = lastWordEnd; //another ++ in the loop
                }
                node.init();
            } else {
                if (node.moreLong && (i < n - 1)) {
                    if(begin < 0)
                        begin = i;
                    if(node.data > 0)
                        lastWordEnd = i;
                } else {
                    if (begin < 0)
                    {
                        if(!type->isSpace(str.c_str()))
                            dest.push_back(str);
                    }
                    else {
                        if(node.data > 0)
                            lastWordEnd = i;
                        else if(lastWordEnd < begin)
                            lastWordEnd = begin;

                        toCombine(trie, type, src, begin, lastWordEnd, dest);
                        begin = -1;
                        i = lastWordEnd;
                    }
                    node.init();
                }
            }
        } //end for

        if(begin >= 0){
            toCombine(trie, type, src, begin, n-1, dest);
        }
    }

}

    void CMA_ME_Analyzer::analysis_mmmodel(
            const char* sentence,
            int N,
            vector<vector<string> >& posRet,
            vector<pair<vector<string>, double> >& segRet,
            bool tagPOS
            )
    {
        // Initial Step 1: split as Chinese Character based
        vector<string> words;
        extractCharacter( sentence, words );

        if( words.empty() == true )
            return;

        // Initial Step 2nd: set character types
        CharType types[ (int)words.size() ];
        setCharType( words, types );

    	int segN = N;
        vector<pair<vector<string>, double> > segment(segN);
        SegTagger* segTagger = knowledge_->getSegTagger();

        if(N == 1)
            segTagger->seg_sentence_best(words, types, segment[0].first);
        else
            segTagger->seg_sentence(words, types, segN, N, segment);

        N = segment.size();
        segRet.resize(N);

        VTrie *trie = knowledge_->getTrie();
        //TODO, only combine the first result
        for (int i = 0; i < N; ++i) {
            pair<vector<string>, double>& srcPair = segment[i];
            pair<vector<string>, double>& destPair = segRet[i];
            destPair.second = srcPair.second;
            if(i < 1){
                meanainner::combineRetWithTrie( trie, srcPair.first,
                        destPair.first, ctype_);
            }
            else{
                size_t srcSize = srcPair.first.size();
                for(size_t si = 0; si < srcSize; ++si){
                    string& val = srcPair.first[si];
                    if(!ctype_->isSpace(val.c_str()))
                        destPair.first.push_back(val);
                }
            }
        }

        if(!tagPOS)
            return;

        posRet.resize(N);
        POSTagger* posTagger = knowledge_->getPOSTagger();       
        for (int i = 0; i < N; ++i) {
            posTagger->tag_sentence_best(segRet[i].first, posRet[i], 0,
                segRet[i].first.size());
        }


    }

    void CMA_ME_Analyzer::analysis_fmm(
            const char* sentence,
            int N,
            vector<vector<string> >& posRet,
            vector<pair<vector<string>, double> >& segRet,
            bool tagPOS
            )
    {
        // Initial Step 1: split as Chinese Character based
        vector<string> words;
        extractCharacter( sentence, words );

        if( words.empty() == true )
            return;
/*
        int maxWordOff = (int)words.size() - 1;
        CharType types[maxWordOff + 1];
        CharType preType = CHAR_TYPE_INIT;
        for(int i=0; i<maxWordOff; ++i){
            types[i] = preType = ctype_->getCharType(words[i].data(),
                    preType, words[i+1].data());

        }

        types[maxWordOff] = ctype_->getCharType(words[maxWordOff].data(),
                    preType, 0);
*/

        segRet.resize(1);

        VTrie *trie = knowledge_->getTrie();
        meanainner::combineRetWithTrie( trie, words, segRet[0].first, ctype_);
        segRet[0].second = 1;

        if(!tagPOS)
            return;

        posRet.resize(1);
        vector<string>& posRetOne = posRet[0];
        vector<set<string> >& posVec = knowledge_->getPOSTagger()->posVec_;
        string& defaultPOS = knowledge_->getPOSTagger()->defaultPOS;
        vector<string>& wordVec = segRet[0].first;
        size_t wordSize = wordVec.size();
        posRetOne.resize( wordSize );
        for (size_t i = 0; i < wordSize; ++i) {
            VTrieNode node;
            trie->search( wordVec[i].data(), &node );
            if( node.data > 0 )
            {
                set<string>& posSet = posVec[node.data];
                if( !posSet.empty() )
                {
                    posRetOne[i] = *posSet.begin();
                    continue;
                }
            }

            posRetOne[i] = defaultPOS;
        }
    }

    void CMA_ME_Analyzer::analysis_dictb(
            const char* sentence,
            int N,
            vector<vector<string> >& posRet,
            vector<pair<vector<string>, double> >& segRet,
            bool tagPOS
            )
    {
        // Initial Step 1: split as Chinese Character based
        vector<string> words;
        extractCharacter( sentence, words );

        if( words.empty() == true )
            return;

        // Initial Step 2nd: set character types
        CharType types[ (int)words.size() ];
        setCharType( words, types );

        int segN = N;
        vector<pair<vector<string>, double> > segment( segN );
        //SegTagger* segTagger = knowledge_->getSegTagger();

        // Segment 1st. Perform special characters



        N = segment.size();
        segRet.resize(N);

        VTrie *trie = knowledge_->getTrie();
        //TODO, only combine the first result
        for (int i = 0; i < N; ++i) {
            pair<vector<string>, double>& srcPair = segment[i];
            pair<vector<string>, double>& destPair = segRet[i];
            destPair.second = srcPair.second;
            if(i < 1){
                meanainner::combineRetWithTrie( trie, srcPair.first,
                        destPair.first, ctype_);
            }
            else{
                size_t srcSize = srcPair.first.size();
                for(size_t si = 0; si < srcSize; ++si){
                    string& val = srcPair.first[si];
                    if(!ctype_->isSpace(val.c_str()))
                        destPair.first.push_back(val);
                }
            }
        }

        if(!tagPOS)
            return;

        posRet.resize(N);
        POSTagger* posTagger = knowledge_->getPOSTagger();
        for (int i = 0; i < N; ++i) {
            posTagger->tag_sentence_best(segRet[i].first, posRet[i], 0,
                segRet[i].first.size());
        }


    }


    void CMA_ME_Analyzer::analysis_fmincover(
            const char* sentence,
            int N,
            vector<vector<string> >& posRet,
            vector<pair<vector<string>, double> >& segment,
            bool tagPOS
            )
    {
        // Initial Step 1: split as Chinese Character based
        vector<string> words;
        extractCharacter( sentence, words );

        if( words.empty() == true )
            return;
        // Initial Step 2nd: set character types
        CharType types[ (int)words.size() ];
        setCharType( words, types );

        segment.resize(1);
        segment[0].second = 1;

        VTrie *trie = knowledge_->getTrie();
        fmincover::parseFMinCoverString(
                segment[0].first, words, types, trie, 0, words.size() );

        if(!tagPOS)
            return;

        posRet.resize(1);
        vector<string>& posRetOne = posRet[0];
        vector<set<string> >& posVec = knowledge_->getPOSTagger()->posVec_;
        string& defaultPOS = knowledge_->getPOSTagger()->defaultPOS;
        vector<string>& wordVec = segment[0].first;
        size_t wordSize = wordVec.size();
        posRetOne.resize( wordSize );
        for (size_t i = 0; i < wordSize; ++i) {
            VTrieNode node;
            trie->search( wordVec[i].data(), &node );
            if( node.data > 0 )
            {
                set<string>& posSet = posVec[node.data];
                if( !posSet.empty() )
                {
                    posRetOne[i] = *posSet.begin();
                    continue;
                }
            }

            posRetOne[i] = defaultPOS;
        }
    }

    void CMA_ME_Analyzer::splitToOneGram( const char* sentence, vector<vector<OneGramType> >& output )
    {
    	if( encodeType_ == Knowledge::ENCODE_TYPE_UTF8 )
		{
    		const unsigned char *uc = (const unsigned char *)sentence;
			if( uc[0] == 0xEF && uc[1] == 0xBB && uc[2] == 0xBF )
				sentence += 3;
		}

    	CTypeTokenizer token(ctype_, sentence);

    	const char* next = 0;

    	output.push_back( vector<OneGramType>() );
    	vector<OneGramType> *curFragment = &output[ output.size() - 1 ];

    	while( (next = token.next()) ){
    		CharType curType = ctype_->getBaseType( next );
    		switch( curType )
    		{
    		case CHAR_TYPE_NUMBER:
    		case CHAR_TYPE_LETTER:
    		case CHAR_TYPE_PUNC:
    			if( !curFragment->empty() )
				{
					output.push_back( vector<OneGramType>() );
					curFragment = &output[ output.size() - 1 ];
				}
				continue;
    		case CHAR_TYPE_SPACE:
    			continue;
    		default:
    			;//do nothing
    		}

			curFragment->push_back( next );
		}
    }

    void CMA_ME_Analyzer::getNGramResultImpl( const vector<vector<OneGramType> >& oneGram, const int n,
    		vector<string>& output )
    {
    	if( n < 1 )
    		return;
    	size_t fragSize = oneGram.size();
    	for(size_t fidx = 0; fidx < fragSize; ++fidx)
    	{
    		const vector<OneGramType>& curFragment = oneGram[ fidx ];
    		int size = static_cast<int>(curFragment.size());
			int lastIdx = size - n + 1;
			//cout<<"size="<<size<<",n="<<n<<",lastIdx="<<lastIdx<<endl;
			for( int i = 0; i < lastIdx; ++i )
			{
				string buf = curFragment[i];
				for( int j = 1; j < n; ++j )
					buf += curFragment[ i + j ];

				output.push_back( buf );
			}
    	}
    }

    void CMA_ME_Analyzer::resetIndexPOSList( bool defVal )
    {
    	posTable_->resetIndexPOSList( defVal );
    }

    int CMA_ME_Analyzer::setIndexPOSList( std::vector<std::string>& posList )
    {
    	return posTable_->setIndexPOSList( posList );
    }

    int CMA_ME_Analyzer::getCodeFromStr( const string& pos ) const
    {
    	return posTable_->getCodeFromStr( pos );
    }

    const char* CMA_ME_Analyzer::getStrFromCode( int index ) const
    {
    	return posTable_->getStrFromCode( index );
    }

    int CMA_ME_Analyzer::getPOSTagSetSize() const
    {
    	return posTable_->size();
    }

    void CMA_ME_Analyzer::extractCharacter( const char* sentence, vector< string >& charOut )
    {
        if( encodeType_ == Knowledge::ENCODE_TYPE_UTF8 )
        {
            const unsigned char *uc = (const unsigned char *)sentence;
            if( uc[0] == 0xEF && uc[1] == 0xBB && uc[2] == 0xBF )
                sentence += 3;
        }

        size_t len = ctype_->length( sentence );
        charOut.resize( len );
        if( len == 0 )
            return;

        size_t i = 0;
        CTypeTokenizer token( ctype_, sentence );
        const char* next = 0;
        while( ( next = token.next() ) )
        {
            charOut[ i ].assign( next );
            ++i;
        }
    }

    void CMA_ME_Analyzer::setCharType( vector< string >& charIn, CharType* types )
    {
        int maxWordOff = (int)charIn.size() - 1;
        CharType preType = CHAR_TYPE_INIT;
        for( int i = 0; i < maxWordOff; ++i )
        {
            types[i] = preType = ctype_->getCharType(
                    charIn[ i ].data(), preType, charIn[i+1].data() );
        }
        types[ maxWordOff ] = ctype_->getCharType(
                charIn[ maxWordOff ].data(), preType, 0);
    }

}