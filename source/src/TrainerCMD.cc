#define TRAINERCMD_AS_MAIN

#include "TrainerCMD.h"

void beginTrain(string pMateFile, string cateFile, string enc, 
        string posDelimiter, bool trainPOS){
    string mateFile = pMateFile;

    Knowledge::EncodeType encType = CMA_CType::getEncType(enc);

    if(trainPOS){
        cout<<"#Start Training POS Model..."<<endl;
        pos_train(mateFile.data(), cateFile, encType, posDelimiter);
    }

    cout<<"#Generate POC Material File "<<endl;
    string matePOC = mateFile + ".poc.tmp";
    create_poc_meterial(mateFile.data(), matePOC.data(), encType, posDelimiter);

    string cateFilePOC = cateFile + "-poc";
    cout<<"#Start Training POC Model..."<<endl;
    poc_train(matePOC.data(), cateFilePOC, encType, posDelimiter);

    //remove matePOC and mateFile(if tmpMateFile is true)
    cout<<"#Remove temporal files "<<endl;
    remove(matePOC.data());

    cout<<"#Training Finished!"<<endl;
}

void printTainerUsage(){
    cout<<"SYNOPSIS\n     ./cmactrainer mateFile cateFile [encoding] [posDelimiter]"<<endl;
    cout<<"Description"<<endl;
    cout<<"   mateFile is the material file, it should be in the form "<<
            "word1/pos1 word2/pos2 word3/pos3 ..."<<endl;
    cout<<"   cateFile is the category file, there are several files are "<<
            "created after the training, and with cateFile as the prefix, "<<
            "prefix should contains both path and name, such /dir1/dir2/n1"<<endl;
    cout<<"   encoding is the encoding of the mateFile, and gb2312"<<
            "is the default encoding. Only support gb2312 and big5 now"<<endl;
    cout<<"   posDelimiter is the delimiter between the word and the pos tag, "<<
            "like '/' and '_' and default is '/'"<<endl;
}

int tainerEntry(int argc, char** argv){
    if(argc < 3){
        printTainerUsage();
        return 0;
    }
    string pMateFile(argv[1]);
    string cateFile(argv[2]);
    string encoding = "gb2312";
    if(argc > 3){
        encoding = argv[3];
    }
    string posDelimiter = "/";
    if(argc > 4){
        posDelimiter = argv[4];
    }
    bool trainPOS = true;
    if(argc > 5){
        trainPOS = strcmp(argv[5],"--notrain");
    }

    beginTrain(pMateFile, cateFile, encoding, posDelimiter, trainPOS);
    return 1;
}

#ifdef TRAINERCMD_AS_MAIN
int main(int argc, char** argv){
    return tainerEntry(argc, argv);
}
#endif
