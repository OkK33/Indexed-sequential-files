#include <iostream>
#include <string>
#include <time.h> 
#include <fstream>
using namespace std;

#define BUFFER_SIZE 4
#define INDEX_BUFFER_SIZE 10
#define LIMIT 100
#define ALPHA 0.5
#define REORG_LVL 0.2

int setOfFiles = 1;
int operations = 0;
int globalOperations = 0;


struct IndexEntry {
    int key = -1;
    int pageNum = -1;
};

//#pragma pack(push, 1) //wyrownanie paddingu do 1 bajta
struct Record {
    int key = -1;
    float voltage = -1.0f;
    float current = -1.0f;
    bool deleted = true;
    int ovPointer = -1;
};
//#pragma pack(pop)

struct DataFile {
    string name;
    Record* buffer = nullptr;
    int currentRecords; //lacznie z usunietymi
    int prevPage; //aktualna strona w buforze
    int currentPages; //dla overflow niepotrzebne
    int deletedRecords = 0;
};

struct IndexFile {
    string name;
    IndexEntry* buffer = nullptr;
    int currentEntries;
    int prevPage;
};

float randFloat()
{
    return (float)((rand()) / (float)(RAND_MAX)) * LIMIT; // podzielenie przez siebie 2 liczb zmiennoprzecinkowych (wynik od 0 do 1) i pomnozenie przez limit
}

void clearDataBuffer(DataFile& data) {
    for (int i = 0; i < BUFFER_SIZE; i++) {
        data.buffer[i] = { -1,-1.0f,-1.0f,true,-1 };
    }
}

void clearIndexBuffer(IndexFile& index) {
    for (int i = 0; i < INDEX_BUFFER_SIZE; i++) {
        index.buffer[i] = { -1,-1 };
    }
}

void sortDataBuffer(DataFile& data) {

    for (int i = 0; i < BUFFER_SIZE; i++) {
        for (int j = 0; j < BUFFER_SIZE - 1 - i; j++) {
            if (data.buffer[j].key > data.buffer[j + 1].key && data.buffer[j + 1].key != -1) {
                Record temp = data.buffer[j + 1];
                data.buffer[j + 1] = data.buffer[j];
                data.buffer[j] = temp;

            }
        }
    }
}

void loadPageToIndexBuffer(IndexFile& index, int pageNumber) {
    fstream indexStream(index.name, ios::binary | ios::in | ios::out);
    clearIndexBuffer(index);
    int startByte = pageNumber * INDEX_BUFFER_SIZE * sizeof(IndexEntry);
    indexStream.seekg(startByte, ios::beg);
    indexStream.read((char*)(&index.buffer[0]), sizeof(IndexEntry) * INDEX_BUFFER_SIZE);
    operations++;
    globalOperations++;
    indexStream.close();
}

//zwraca numer strony na ktora powinien wstawic nowy rekord
int findPageInIndex(IndexFile& index, int key) {

    IndexEntry lastEntryInPrevBuffer = { INT_MAX,INT_MAX };
    int pagesInIndex = ceil((float)index.currentEntries / INDEX_BUFFER_SIZE);


    for (int i = 0; i < pagesInIndex; i++) {
        loadPageToIndexBuffer(index, i);
        if (index.buffer[0].key > key && lastEntryInPrevBuffer.key <= key) { //jesli klucz jest wiekszy od tego w ostatnim wpisie z poprzedniej strony i mniejszy od pierwszego w nowej stronie
            return lastEntryInPrevBuffer.pageNum;
        }
        for (int j = 0; j < INDEX_BUFFER_SIZE - 1; j++) {
            if (index.buffer[j].key <= key && index.buffer[j + 1].pageNum == -1) { //jesli klucz jest wiekszy od tego we wpisie i nie ma nastepnego wpisu
                return index.buffer[j].pageNum;
            }
            else if (index.buffer[j].key <= key && index.buffer[j + 1].key > key) { //jesli klucz jest wiekszy od tego we wpisie i mniejszy od nastepnego
                return index.buffer[j].pageNum;
            }

        }
        if (index.buffer[INDEX_BUFFER_SIZE - 1].key <= key && i == pagesInIndex - 1) //jesli nie ma nastepnej strony
        {
            return index.buffer[INDEX_BUFFER_SIZE - 1].pageNum;
        }
        lastEntryInPrevBuffer = index.buffer[INDEX_BUFFER_SIZE - 1];
    }

    return -1;
}


void loadPageToDataBuffer(DataFile& data, int pageNumber) {
    fstream dataStream(data.name, ios::binary | ios::in | ios::out);
    clearDataBuffer(data);

    int startByte = pageNumber * BUFFER_SIZE * sizeof(Record);
    dataStream.seekg(startByte, ios::beg);
    dataStream.read((char*)(&data.buffer[0]), sizeof(Record) * BUFFER_SIZE);
    operations++;
    globalOperations++;
    dataStream.close();
}


void writeToDataFile(DataFile& data, int pageNumber) {
    fstream dataStream(data.name, ios::binary | ios::in | ios::out);
    int startByte = pageNumber * BUFFER_SIZE * sizeof(Record);
    dataStream.seekp(startByte, ios::beg);
    dataStream.write((char*)(&data.buffer[0]), sizeof(Record) * BUFFER_SIZE);
    operations++;
    globalOperations++;
    dataStream.close();

}

void writeToIndexFile(IndexFile& index, int pageNumber) {
    fstream indexStream(index.name, ios::binary | ios::in | ios::out);
    int startByte = pageNumber * INDEX_BUFFER_SIZE * sizeof(IndexEntry);
    indexStream.seekp(startByte, ios::beg);
    indexStream.write((char*)(&index.buffer[0]), sizeof(IndexEntry) * INDEX_BUFFER_SIZE);
    operations++;
    globalOperations++;
    indexStream.close();
}


//zwraca pierwszy wolny numer slotu na stronie, jesli nie ma wolnych to -1
int firstDataPageFreeSlot(DataFile& data) {
    for (int i = 0; i < BUFFER_SIZE; i++) {
        if (data.buffer[i].key == -1) {
            return i;
        }
    }
    return -1;
}

int countAllRecords(DataFile& data, DataFile& overflow) {
    return data.currentRecords + overflow.currentRecords;
}

bool addToOverflow(DataFile& overflow, DataFile& data, Record newRecord, int pageNumber) {


    for (int i = BUFFER_SIZE - 1; i >= 0; i--) {
        if (newRecord.key == data.buffer[i].key && data.buffer[i].deleted == false) {
            return false;
        }
        if (newRecord.key == data.buffer[i].key && data.buffer[i].deleted == true) {

            newRecord.ovPointer = data.buffer[i].ovPointer;
            data.buffer[i] = newRecord;
            writeToDataFile(data, pageNumber);
            data.deletedRecords--;
            return true;
        }
        if (newRecord.key > data.buffer[i].key && data.buffer[i].ovPointer == -1) { //wpisanie do overflow jesli jest pusty
            data.buffer[i].ovPointer = overflow.currentRecords;
            writeToDataFile(data, pageNumber);
            int startPage = overflow.currentRecords / BUFFER_SIZE;
            int startByte = startPage * BUFFER_SIZE * sizeof(Record);
            loadPageToDataBuffer(overflow, startPage);
            overflow.prevPage = startPage;
            for (int j = 0; j < BUFFER_SIZE; j++) {
                if (overflow.buffer[j].key == -1) {
                    overflow.buffer[j] = newRecord;
                    overflow.currentRecords++;
                    break;
                }
            }
            writeToDataFile(overflow, startPage);
            return true;
        }
        else if (newRecord.key > data.buffer[i].key && data.buffer[i].ovPointer != -1) { //jesli overflowPointer nie jest pusty
            int prevRecordLocation = data.buffer[i].ovPointer;
            Record prevRecord;
            int page = prevRecordLocation / BUFFER_SIZE;
            int recordInPage = prevRecordLocation % BUFFER_SIZE;
            if (page != overflow.prevPage) {
                loadPageToDataBuffer(overflow, page);
                overflow.prevPage = page;
            }

            prevRecord = overflow.buffer[recordInPage];
            int depth = 0;

            if (newRecord.key == prevRecord.key && prevRecord.deleted == false) {
                return false;
            }
            if (newRecord.key == prevRecord.key && prevRecord.deleted == true) {

                newRecord.ovPointer = prevRecord.ovPointer;
                page = prevRecordLocation / BUFFER_SIZE;
                recordInPage = prevRecordLocation % BUFFER_SIZE;
                if (page != overflow.prevPage) {
                    loadPageToDataBuffer(overflow, page);
                    overflow.prevPage = page;
                }
                overflow.buffer[recordInPage] = newRecord;
                writeToDataFile(overflow, page);
                overflow.deletedRecords--;
                return true;
            }

            while (true) {
                if (newRecord.key == prevRecord.key && prevRecord.deleted == false) {
                    return false;
                }
                if (newRecord.key == prevRecord.key && prevRecord.deleted == true) {

                    newRecord.ovPointer = prevRecord.ovPointer;
                    page = prevRecordLocation / BUFFER_SIZE;
                    recordInPage = prevRecordLocation % BUFFER_SIZE;
                    if (page != overflow.prevPage) {
                        loadPageToDataBuffer(overflow, page);
                        overflow.prevPage = page;
                    }
                    overflow.buffer[recordInPage] = newRecord;
                    writeToDataFile(overflow, page);
                    overflow.deletedRecords--;
                    return true;
                }

                if (depth == 0) { //jesli 0 poziom wglebienia
                    if (newRecord.key < prevRecord.key) {
                        newRecord.ovPointer = data.buffer[i].ovPointer;
                        data.buffer[i].ovPointer = overflow.currentRecords;
                        writeToDataFile(data, pageNumber);
                        page = overflow.currentRecords / BUFFER_SIZE;
                        if (page != overflow.prevPage) {
                            loadPageToDataBuffer(overflow, page);
                            overflow.prevPage = page;
                        }
                        int firstFreeSlot = firstDataPageFreeSlot(overflow);
                        overflow.buffer[firstFreeSlot] = newRecord;
                        writeToDataFile(overflow, page);
                        overflow.currentRecords++;
                        return true;
                    }

                }
                else {

                    if (newRecord.key > prevRecord.key) {
                        if (prevRecord.ovPointer == -1) {
                            newRecord.ovPointer = prevRecord.ovPointer;
                            page = prevRecordLocation / BUFFER_SIZE;
                            recordInPage = prevRecordLocation % BUFFER_SIZE;
                            if (page != overflow.prevPage) {
                                loadPageToDataBuffer(overflow, page);
                                overflow.prevPage = page;
                            }
                            overflow.buffer[recordInPage].ovPointer = overflow.currentRecords; //zaktualizowanie poprzedniego
                            writeToDataFile(overflow, page);

                            page = overflow.currentRecords / BUFFER_SIZE; //wspisanie nowego
                            recordInPage = overflow.currentRecords % BUFFER_SIZE;
                            if (page != overflow.prevPage) {
                                loadPageToDataBuffer(overflow, page);
                                overflow.prevPage = page;
                            }
                            int firstFreeSlot = firstDataPageFreeSlot(overflow);
                            overflow.buffer[firstFreeSlot] = newRecord;
                            writeToDataFile(overflow, page);
                            overflow.currentRecords++;
                            return true;
                        }
                        else {
                            page = prevRecord.ovPointer / BUFFER_SIZE;
                            recordInPage = prevRecord.ovPointer % BUFFER_SIZE;
                            if (page != overflow.prevPage) {
                                loadPageToDataBuffer(overflow, page);
                                overflow.prevPage = page;
                            }
                            if (overflow.buffer[recordInPage].key > newRecord.key) {
                                newRecord.ovPointer = prevRecord.ovPointer;
                                page = prevRecordLocation / BUFFER_SIZE;
                                recordInPage = prevRecordLocation % BUFFER_SIZE;
                                if (page != overflow.prevPage) {
                                    loadPageToDataBuffer(overflow, page);
                                    overflow.prevPage = page;
                                }
                                overflow.buffer[recordInPage].ovPointer = overflow.currentRecords; //zaktualizowanie poprzedniego
                                writeToDataFile(overflow, page);

                                page = overflow.currentRecords / BUFFER_SIZE; //wspisanie nowego
                                recordInPage = overflow.currentRecords % BUFFER_SIZE;
                                if (page != overflow.prevPage) {
                                    loadPageToDataBuffer(overflow, page);
                                    overflow.prevPage = page;
                                }
                                int firstFreeSlot = firstDataPageFreeSlot(overflow);
                                overflow.buffer[firstFreeSlot] = newRecord;
                                writeToDataFile(overflow, page);
                                overflow.currentRecords++;

                                return true;
                            }
                            else {
                                prevRecordLocation = prevRecord.ovPointer;
                                prevRecord = overflow.buffer[recordInPage];
                                
                            }
                        }



                    }
                    

                }

                depth++;
            }

        }

    }
    return false;

}





Record findRecord(IndexFile& index, DataFile& data, DataFile& overflow, int key) {
    Record record;
    int pageNumber = findPageInIndex(index, key);
    if (pageNumber != data.prevPage) {
        loadPageToDataBuffer(data, pageNumber);
        data.prevPage = pageNumber;
    }
    for (int i = 0; i < BUFFER_SIZE; i++) {
        if (data.buffer[i].key != -1) {
            if (data.buffer[i].key == key) {
                return data.buffer[i]; //jesli klucz jest w main area
            }
            if (data.buffer[i].key < key) {
                record = data.buffer[i];
                //break;
            }
        }

    }
    while (record.key != key) {
        if (record.ovPointer == -1) {
            Record record2;
            return record2;
        }
        int nextOverflowPage = record.ovPointer / BUFFER_SIZE;
        int nextOverflowPlaceInBuffer = record.ovPointer % BUFFER_SIZE;
        if (nextOverflowPage != overflow.prevPage) {
            loadPageToDataBuffer(overflow, nextOverflowPage);
            overflow.prevPage = nextOverflowPage;
        }
        record = overflow.buffer[nextOverflowPlaceInBuffer];
        if (record.key == key) {
            return record;
        }
        if (record.ovPointer == -1) {
            Record record2;
            return record2;
        }

    }
    return record;
}

bool deleteRecord(IndexFile& index, DataFile& data, DataFile& overflow, int key) {
    Record record = findRecord(index, data, overflow, key);
    if (record.key == -1 || (record.key != -1 && record.deleted == true)) {
        printf("Record with this key doesn't exist \n");
        return false;
    }
    else {
        int pageNumber = findPageInIndex(index, key);
        if (pageNumber != data.prevPage) {
            loadPageToDataBuffer(data, pageNumber);
            data.prevPage = pageNumber;
        }
        for (int i = 0; i < BUFFER_SIZE; i++) {
            if (data.buffer[i].key == key) {
                data.buffer[i].deleted = true; //jesli klucz jest w main area
                writeToDataFile(data, pageNumber);
                data.deletedRecords++;
                return true;
            }
            if (data.buffer[i].key < key) {
                record = data.buffer[i];
                // break;
            }
        }
        while (record.key != key) {
            int nextOverflowPage = record.ovPointer / BUFFER_SIZE;
            int nextOverflowPlaceInBuffer = record.ovPointer % BUFFER_SIZE;
            if (nextOverflowPage != overflow.prevPage) {
                loadPageToDataBuffer(overflow, nextOverflowPage);
                overflow.prevPage = nextOverflowPage;
            }
            record = overflow.buffer[nextOverflowPlaceInBuffer];
            if (record.key == key) {
                overflow.buffer[nextOverflowPlaceInBuffer].deleted = true;
                writeToDataFile(overflow, nextOverflowPage);
                overflow.deletedRecords++;
                return true;
            }


        }
    }
}

void showAllRecords(IndexFile& index, DataFile& data, DataFile& overflow) {
    Record recordToPrint;
    int recordCounter = 0;
    for (int i = 0; i < ceil((float)index.currentEntries / INDEX_BUFFER_SIZE); i++) {
        if (index.prevPage != i) {
            loadPageToIndexBuffer(index, i);
            index.prevPage = i;
        }

        for (int j = 0; j < INDEX_BUFFER_SIZE; j++) {
            int dataPage = index.buffer[j].pageNum;
            if (dataPage != -1) {
                if (data.prevPage != dataPage) {
                    loadPageToDataBuffer(data, dataPage);
                    data.prevPage = dataPage;
                }
                for (int k = 0; k < BUFFER_SIZE; k++) {
                    if (data.buffer[k].key != -1) {
                        recordToPrint = data.buffer[k];
                        if (recordToPrint.key != -2 && recordToPrint.deleted == false) {
                            printf("%d. KEY: %d      DATA: %f, %f        DEL: %i   OV: %i \n", recordCounter, recordToPrint.key, recordToPrint.voltage, recordToPrint.current, recordToPrint.deleted, recordToPrint.ovPointer);
                            recordCounter++;
                        }
                        while (recordToPrint.ovPointer != -1) {
                            int nextRecordLocation = recordToPrint.ovPointer;
                            int overflowPage = nextRecordLocation / BUFFER_SIZE;
                            int placeInBuffer = nextRecordLocation % BUFFER_SIZE;
                            if (overflowPage != overflow.prevPage) {
                                loadPageToDataBuffer(overflow, overflowPage);
                                overflow.prevPage = overflowPage;
                            }
                            recordToPrint = overflow.buffer[placeInBuffer];
                            if (recordToPrint.key != -2 && recordToPrint.deleted == false) {
                                printf("%d. KEY: %d      DATA: %f, %f        DEL: %i   OV: %i \n", recordCounter, recordToPrint.key, recordToPrint.voltage, recordToPrint.current, recordToPrint.deleted, recordToPrint.ovPointer);
                                recordCounter++;
                            }
                        }
                    }
                }


            }
        }
    }
}


//tworzenie indexu z jednym wpisem i wpisanie rekordu o kluczu 0 jako pierwsza strone
void initFiles(IndexFile& index, DataFile& data) {

    IndexEntry entry = { -2,0 };
    Record record = { -2,-1.0f,-1.0f,true,-1 };


    data.buffer[0] = record;
    index.buffer[0] = entry;
    writeToDataFile(data, 0);
    writeToIndexFile(index, 0);

    data.currentRecords = 1;
    index.currentEntries = 1;
    data.currentPages = 1;
    data.prevPage = 0;
    index.prevPage = 0;


}

void printAll(IndexFile& index, DataFile& data, DataFile& overflow) {

    ifstream indexStream(index.name, ios::binary);
    ifstream dataStream(data.name, ios::binary);
    ifstream overflowStream(overflow.name, ios::binary);
    IndexEntry entry;
    Record record;
    int overflowCounter = 0;
    int dataCounter = 0;
    int indexCounter = 0;
    printf("Index file: \n");
    printf("  KEY:      PAGE_NUMBER: \n");
    while (indexStream.read((char*)(&entry), sizeof(IndexEntry))) {
        printf("%d.  %d          %d \n", indexCounter, entry.key, entry.pageNum);
        indexCounter++;
    }
    printf("Data file: \n");
    printf("  KEY:    DATA:                      DEL:  OV: \n");

    while (dataStream.read((char*)(&record), sizeof(Record))) {
        
        printf("%d.  %d      %f, %f         %i    %i \n", dataCounter, record.key, record.voltage, record.current, record.deleted, record.ovPointer);
        dataCounter++;
    }
    printf("Overflow file: \n");
    printf("  KEY:    DATA:                      DEL:  OV: \n");
    while (overflowStream.read((char*)(&record), sizeof(Record))) {
        printf("%d.  %d      %f, %f         %i    %i \n", overflowCounter, record.key, record.voltage, record.current, record.deleted, record.ovPointer);
        overflowCounter++;
    }

    indexStream.close();
    dataStream.close();
    overflowStream.close();
}

void beforeReorg(int dataPages, int newIndexPages, int newDataPages, int newOverflowPages, DataFile& data, IndexFile& index, DataFile& overflow, float alpha) {
    

    IndexEntry* indexBuffer = new IndexEntry[INDEX_BUFFER_SIZE];
    Record* dataBuffer = new Record[BUFFER_SIZE];
    Record* overflowBuffer = new Record[BUFFER_SIZE];

    DataFile dataFile = { "data2.bin", dataBuffer,0,-1,0,0 };
    DataFile overflowFile = { "overflow2.bin", overflowBuffer,0,-1,0,0 };
    IndexFile indexFile = { "index2.bin", indexBuffer, 0 };

    if (setOfFiles == 2) {
        dataFile.name = "data.bin";
        overflowFile.name = "overflow.bin";
        indexFile.name = "index.bin";
    }


    ofstream file1(indexFile.name, std::ios::binary | std::ios::trunc);
    ofstream file2(dataFile.name, std::ios::binary | std::ios::trunc);
    ofstream file3(overflowFile.name, std::ios::binary | std::ios::trunc);
    file1.close();
    file2.close();
    file3.close();

    clearDataBuffer(dataFile);
    clearDataBuffer(overflowFile);
    clearIndexBuffer(indexFile);

    initFiles(indexFile, dataFile);
    for (int i = 0; i < newIndexPages; i++) { //alokacja indeksu
        if (i == 0) {
            indexFile.buffer[0] = { -2,0 };
            writeToIndexFile(indexFile, i);
            clearIndexBuffer(indexFile);
        }
        else {
            writeToIndexFile(indexFile, i);
        }
    }
    for (int i = 0; i < newDataPages; i++) { //alokacja mainArea

        if (i == 0) {
            dataFile.buffer[0] = { -2,-1.0f,-1.0f,true,-1 };
            writeToDataFile(dataFile, i);
            clearDataBuffer(dataFile);
        }
        else {
            writeToDataFile(dataFile, i);
            dataFile.currentPages++;
        }
    }
    for (int i = 0; i < newOverflowPages; i++) { //alokacja overflowArea
        writeToDataFile(overflowFile, i);
    }


    int rewrittenRecords = 1;
    if (data.prevPage != 0) {
        loadPageToDataBuffer(data, 0);
        data.prevPage = 0;
    }
    dataFile.buffer[0] = { -2,-1.0f,-1.0f,true,-1 };
    indexFile.buffer[0] = { -2,0 };
    Record recordToWrite;
    int currentDataPageToWrite = 0;
    int recordsInCurrentPage = 1; //jest jeden poczatkowy
    int currentIndexPageToWrite = 0;

    for (int j = 0; j < dataPages; j++) {

        loadPageToDataBuffer(data, j);
        data.prevPage = j;
        for (int i = 0; i < BUFFER_SIZE; i++) {
            if (data.buffer[i].key != -1) { //przepisuj tylko nie usuniete, poza pierwszym rekordem
                if (data.buffer[i].ovPointer == -1) {
                    recordToWrite = data.buffer[i];
                    recordToWrite.ovPointer = -1;

                    if (recordsInCurrentPage < alpha * BUFFER_SIZE) {

                        if (dataFile.prevPage != currentDataPageToWrite) {
                            loadPageToDataBuffer(dataFile, currentDataPageToWrite);
                            dataFile.prevPage = currentDataPageToWrite;
                        }
                        if (recordToWrite.deleted == false)
                        {

                            int firstFreeSlot = firstDataPageFreeSlot(dataFile);
                            dataFile.buffer[firstFreeSlot] = recordToWrite;
                            writeToDataFile(dataFile, currentDataPageToWrite);
                            recordsInCurrentPage++;
                            rewrittenRecords++;
                            if (recordToWrite.key != -2) { //jest juz policzony
                                dataFile.currentRecords++;
                            }
                        }



                    }
                    else {

                        if (recordToWrite.deleted == false)
                        {

                            currentDataPageToWrite++;
                            recordsInCurrentPage = 0;
                            IndexEntry entry;
                            entry.pageNum = currentDataPageToWrite;
                            entry.key = recordToWrite.key;
                            currentIndexPageToWrite = indexFile.currentEntries / INDEX_BUFFER_SIZE;
                            if (indexFile.prevPage != currentIndexPageToWrite) {
                                loadPageToIndexBuffer(indexFile, currentIndexPageToWrite);
                                indexFile.prevPage = currentIndexPageToWrite;
                            }
                            indexFile.buffer[indexFile.currentEntries % INDEX_BUFFER_SIZE] = entry;
                            writeToIndexFile(indexFile, currentIndexPageToWrite);
                            indexFile.currentEntries++;

                            if (dataFile.prevPage != currentDataPageToWrite) {
                                loadPageToDataBuffer(dataFile, currentDataPageToWrite);
                                dataFile.prevPage = currentDataPageToWrite;
                            }
                            int firstFreeSlot = firstDataPageFreeSlot(dataFile);
                            dataFile.buffer[firstFreeSlot] = recordToWrite;
                            writeToDataFile(dataFile, currentDataPageToWrite);
                            recordsInCurrentPage++;
                            rewrittenRecords++;
                            if (recordToWrite.key != -2) { //jest juz policzony
                                dataFile.currentRecords++;
                            }
                        }
                        //printAll(indexFile, dataFile, overflowFile);


                    }

                }
                else if (data.buffer[i].ovPointer != -1) {
                    int ovPointer = data.buffer[i].ovPointer;

                    recordToWrite = data.buffer[i]; //wpisywanie z main area
                    int newOvPointer = recordToWrite.ovPointer;
                    recordToWrite.ovPointer = -1;
                    if (recordsInCurrentPage < alpha * BUFFER_SIZE) {

                        if (dataFile.prevPage != currentDataPageToWrite) {
                            loadPageToDataBuffer(dataFile, currentDataPageToWrite);
                            dataFile.prevPage = currentDataPageToWrite;
                        }
                        if (recordToWrite.deleted == false)
                        {
                            int firstFreeSlot = firstDataPageFreeSlot(dataFile);
                            dataFile.buffer[firstFreeSlot] = recordToWrite;
                            writeToDataFile(dataFile, currentDataPageToWrite);
                            recordsInCurrentPage++;
                            rewrittenRecords++;
                            if (recordToWrite.key != -2) { //jest juz policzony
                                dataFile.currentRecords++;
                            }
                        }
                        ovPointer = newOvPointer;
                        //printAll(indexFile, dataFile, overflowFile);
                    }
                    else {

                        if (recordToWrite.deleted == false)
                        {
 
                            currentDataPageToWrite++;
                            recordsInCurrentPage = 0;
                            IndexEntry entry;
                            entry.pageNum = currentDataPageToWrite;
                            entry.key = recordToWrite.key;
                            currentIndexPageToWrite = indexFile.currentEntries / INDEX_BUFFER_SIZE;
                            if (indexFile.prevPage != currentIndexPageToWrite) {
                                loadPageToIndexBuffer(indexFile, currentIndexPageToWrite);
                                indexFile.prevPage = currentIndexPageToWrite;
                            }
                            indexFile.buffer[indexFile.currentEntries % INDEX_BUFFER_SIZE] = entry;
                            writeToIndexFile(indexFile, currentIndexPageToWrite);
                            indexFile.currentEntries++;
 
                            if (dataFile.prevPage != currentDataPageToWrite) {
                                loadPageToDataBuffer(dataFile, currentDataPageToWrite);
                                dataFile.prevPage = currentDataPageToWrite;
                            }
                            int firstFreeSlot = firstDataPageFreeSlot(dataFile);
                            dataFile.buffer[firstFreeSlot] = recordToWrite;
                            writeToDataFile(dataFile, currentDataPageToWrite);
                            recordsInCurrentPage++;
                            rewrittenRecords++;
                            if (recordToWrite.key != -2) { //jest juz policzony
                                dataFile.currentRecords++;
                            }
                        }
                        ovPointer = newOvPointer;
                        //printAll(indexFile, dataFile, overflowFile);


                    }

                    while (ovPointer != -1) {


                        int overflowPage = ovPointer / BUFFER_SIZE;
                        int overflowRecordInPage = ovPointer % BUFFER_SIZE;

                        if (overflow.prevPage != overflowPage) {
                            loadPageToDataBuffer(overflow, overflowPage);
                            overflow.prevPage = overflowPage;
                        }
                        recordToWrite = overflow.buffer[overflowRecordInPage];
                        int newOvPointer = recordToWrite.ovPointer;
                        recordToWrite.ovPointer = -1;

                        if (recordsInCurrentPage < alpha * BUFFER_SIZE) {

                            if (dataFile.prevPage != currentDataPageToWrite) {
                                loadPageToDataBuffer(dataFile, currentDataPageToWrite);
                                dataFile.prevPage = currentDataPageToWrite;
                            }
                            if (recordToWrite.deleted == false)
                            {
                                int firstFreeSlot = firstDataPageFreeSlot(dataFile);
                                dataFile.buffer[firstFreeSlot] = recordToWrite;
                                writeToDataFile(dataFile, currentDataPageToWrite);
                                recordsInCurrentPage++;
                                rewrittenRecords++;
                                if (recordToWrite.key != -2) { //jest juz policzony
                                    dataFile.currentRecords++;
                                }
                            }
                            ovPointer = newOvPointer;
                            //printAll(indexFile, dataFile, overflowFile);
                        }
                        else {

                            if (recordToWrite.deleted == false)
                            {

                                currentDataPageToWrite++;
                                recordsInCurrentPage = 0;
                                IndexEntry entry;
                                entry.pageNum = currentDataPageToWrite;
                                entry.key = recordToWrite.key;
                                currentIndexPageToWrite = indexFile.currentEntries / INDEX_BUFFER_SIZE;
                                if (indexFile.prevPage != currentIndexPageToWrite) {
                                    loadPageToIndexBuffer(indexFile, currentIndexPageToWrite);
                                    indexFile.prevPage = currentIndexPageToWrite;
                                }
                                indexFile.buffer[indexFile.currentEntries % INDEX_BUFFER_SIZE] = entry;
                                writeToIndexFile(indexFile, currentIndexPageToWrite);
                                indexFile.currentEntries++;

                                if (dataFile.prevPage != currentDataPageToWrite) {
                                    loadPageToDataBuffer(dataFile, currentDataPageToWrite);
                                    dataFile.prevPage = currentDataPageToWrite;
                                }
                                int firstFreeSlot = firstDataPageFreeSlot(dataFile);
                                dataFile.buffer[firstFreeSlot] = recordToWrite;
                                writeToDataFile(dataFile, currentDataPageToWrite);
                                recordsInCurrentPage++;
                                rewrittenRecords++;
                                if (recordToWrite.key != -2) { //jest juz policzony
                                    dataFile.currentRecords++;
                                }

                            }
                            ovPointer = newOvPointer;
                            //printAll(indexFile, dataFile, overflowFile);


                        }
                    }

                }

            }

        }

    }



    delete[] data.buffer;
    delete[] index.buffer;
    delete[] overflow.buffer;
    data = dataFile;
    index = indexFile;
    overflow = overflowFile;
    //printAll(index, data, overflow);

}

void reorganize(IndexFile& index, DataFile& data, DataFile& overflow, float alpha) {
    operations = 0;
    printf("----------REORGANIZING---------- \n");
    int dataPages = data.currentPages;
    int overflowPages = ceil((float)((float)(overflow.currentRecords - overflow.deletedRecords) / (float)BUFFER_SIZE));

    int newDataPages = ceil((float)((float)(countAllRecords(data, overflow) - data.deletedRecords - overflow.deletedRecords) / ((float)BUFFER_SIZE * alpha)));
    int newIndexPages = ceil((float)((float)newDataPages / (float)INDEX_BUFFER_SIZE));
    int newOverflowPages = ceil((float)((float)newDataPages * (float)REORG_LVL));

    beforeReorg(dataPages, newIndexPages, newDataPages, newOverflowPages, data, index, overflow, alpha);
    printf("Reorg used %d operations \n", operations);
    operations = 0;


    if (setOfFiles == 1) {
        setOfFiles = 2;
    }
    else if (setOfFiles == 2) {
        setOfFiles = 1;
    }
}

bool addRecord(DataFile& data, IndexFile& index, DataFile& overflow) {


    int key;
    float voltage, current;
    cin >> key >> voltage >> current;
    Record newRecord = { key,voltage,current,false,-1 };

    int pageNumber = findPageInIndex(index, key);
    if (data.prevPage != pageNumber) {
        loadPageToDataBuffer(data, pageNumber);
        data.prevPage = pageNumber;
    }


    bool alreadyAdded = false;
    for (int i = 0; i < BUFFER_SIZE; i++) { //wpisanie jesli jest miejce na stronie
        if (data.buffer[i].key == -1) {
            data.buffer[i] = newRecord;
            alreadyAdded = true;
            data.currentRecords++;


            break;
        }
        if (data.buffer[i].key == key && data.buffer[i].deleted == false) {
            printf("Adding used %d operations \n", operations);
            operations = 0;
            return false;
        }
        if (data.buffer[i].key == key && data.buffer[i].deleted == true) {
            newRecord.ovPointer = data.buffer[i].ovPointer;
            data.buffer[i] = newRecord;
            alreadyAdded = true;
            data.deletedRecords--;
            break;
        }
    }

    if (alreadyAdded) {
        sortDataBuffer(data);
        writeToDataFile(data, pageNumber);
        printf("Adding used %d operations \n", operations);
        operations = 0;
        if (overflow.currentRecords > ceil(REORG_LVL * (float)data.currentPages) * BUFFER_SIZE) {
            reorganize(index, data, overflow, ALPHA);
        }
    }

    if (!alreadyAdded) { //jak nie ma miejsca na stronie to do overflow
        bool temp = addToOverflow(overflow, data, newRecord, pageNumber);
        printf("Adding used %d operations \n", operations);
        operations = 0;
        if (overflow.currentRecords > ceil(REORG_LVL * (float)data.currentPages) * BUFFER_SIZE) {
            reorganize(index, data, overflow, ALPHA);
        }
        return temp;
    }


    return true;


}

void updateRecord(DataFile& data, DataFile& overflow, IndexFile& index, int key, float voltage, float current) {
    Record record = findRecord(index, data, overflow, key);
    if (record.key == -1 || (record.key != -1 && record.deleted == true)) {
        printf("Record with this key doesn't exist \n");
        return;
    }
    else {
        int pageNumber = findPageInIndex(index, key);
        if (pageNumber != data.prevPage) {
            loadPageToDataBuffer(data, pageNumber);
            data.prevPage = pageNumber;
        }
        for (int i = 0; i < BUFFER_SIZE; i++) {
            if (data.buffer[i].key == key) {
                data.buffer[i].voltage = voltage; //jesli klucz jest w main area
                data.buffer[i].current = current;
                writeToDataFile(data, pageNumber);
                return;
            }
            if (data.buffer[i].key < key) {
                record = data.buffer[i];
            }
        }
        while (record.key != key) {
            int nextOverflowPage = record.ovPointer / BUFFER_SIZE;
            int nextOverflowPlaceInBuffer = record.ovPointer % BUFFER_SIZE;
            if (nextOverflowPage != overflow.prevPage) {
                loadPageToDataBuffer(overflow, nextOverflowPage);
                overflow.prevPage = nextOverflowPage;
            }
            record = overflow.buffer[nextOverflowPlaceInBuffer];
            if (record.key == key) {
                overflow.buffer[nextOverflowPlaceInBuffer].voltage = voltage;
                overflow.buffer[nextOverflowPlaceInBuffer].current = current;
                writeToDataFile(overflow, nextOverflowPage);
                return;
            }


        }
    }
}

void updateWithKey(DataFile& data, DataFile& overflow, IndexFile& index, int key) {
    if (deleteRecord(index, data, overflow, key)) {
        printf("Deleting used %d operations \n", operations);
        operations = 0;
        addRecord(data, index, overflow);
    }
    
}

void initInteractive(DataFile& dataFile, IndexFile& indexFile, DataFile& overflowFile) {
    string command;

    while (cin >> command) {
        
        if (command == "add") {
            if (addRecord(dataFile, indexFile, overflowFile)) {
                /*printf("Adding used %d operations \n", operations);
                operations = 0;*/
                printAll(indexFile, dataFile, overflowFile);
            }
            else {
                //printf("Adding used %d operations \n", operations);
                operations = 0;
                printf("Record with this key already exists \n");
            }
        }
        else if (command == "find") {
            int key;
            cin >> key;
            Record record = findRecord(indexFile, dataFile, overflowFile, key);
            if (record.key != -1 && record.deleted == false) {
                printf("Finding used %d operations \n", operations);
                operations = 0;
                printf("KEY: %d      DATA: %f, %f        DEL: %i   OV: %i \n", record.key, record.voltage, record.current, record.deleted, record.ovPointer);
            }
            else {
                printf("Finding used %d operations \n", operations);
                operations = 0;
                printf("Record with this key doesn't exist \n");
            }
        }
        else if (command == "reorg") {
            reorganize(indexFile, dataFile, overflowFile, ALPHA);
            //printf("Reorg used %d operations \n", operations);
            operations = 0;
        }
        else if (command == "print") {
            printAll(indexFile, dataFile, overflowFile);
        }
        else if (command == "show") {
            showAllRecords(indexFile, dataFile, overflowFile);
            printf("Showing used %d operations \n", operations);
            operations = 0;
        }
        else if (command == "delete") {
            int key;
            cin >> key;
            deleteRecord(indexFile, dataFile, overflowFile, key);
            printf("Deleting used %d operations \n", operations);
            operations = 0;
        }
        else if (command == "update") {
            int key;
            float voltage, current;
            cin >> key >> voltage >> current;
            updateRecord(dataFile, overflowFile, indexFile, key, voltage, current);
            printf("Updating data used %d operations \n", operations);
            operations = 0;
        }
        else if (command == "updateKey") {
            printf("Enter the key of record that you want to change: ");
            int key;
            cin >> key;
            updateWithKey(dataFile, overflowFile, indexFile, key);
            operations = 0;
        }
        else if (command == "operations") {
            printf("All operations: %d \n", globalOperations);
        }
        else if (command == "x") {
            break;
        }
    }
}



int main()
{

    srand(time(NULL));

    IndexEntry* indexBuffer = new IndexEntry[INDEX_BUFFER_SIZE];
    Record* dataBuffer = new Record[BUFFER_SIZE];
    Record* overflowBuffer = new Record[BUFFER_SIZE];

    DataFile dataFile = { "data.bin", dataBuffer,0,-1,0,0 };
    DataFile overflowFile = { "overflow.bin", overflowBuffer,0,-1,0,0 };
    IndexFile indexFile = { "index.bin", indexBuffer, 0, -1 };


    ofstream file1(indexFile.name, std::ios::binary | std::ios::trunc);
    ofstream file2(dataFile.name, std::ios::binary | std::ios::trunc);
    ofstream file3(overflowFile.name, std::ios::binary | std::ios::trunc);
    file1.close();
    file2.close();
    file3.close();

    clearDataBuffer(dataFile);
    clearDataBuffer(overflowFile);
    clearIndexBuffer(indexFile);


    initFiles(indexFile, dataFile);

    initInteractive(dataFile, indexFile, overflowFile);

    return 0;
}