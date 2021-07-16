
/*
 *   Copyright 2021 Huawei Technologies Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>
#include "rwconflict.hpp"

using namespace lpf::debug;

TEST( RWConflict, empty )
{
    ReadWriteConflict c;

    EXPECT_TRUE( c.empty() );
    EXPECT_EQ( 0ul, c.chunkCount() );

    c.clear();

    EXPECT_TRUE( c.empty() );
}

TEST( RWConflict, writeAfter )
{
    ReadWriteConflict c;
    EXPECT_TRUE( c.empty() );

    char xs[] = "Hallo, dit is een test";
    //           0123456789012345678901
    //           0000000000111111111122
    //  old block    ------
    //  write block           -----             

    c.insertRead( xs + 4, 6);

    EXPECT_FALSE( c.checkWrite( xs + 13, 5 ) );
    EXPECT_FALSE( c.empty() );
    EXPECT_EQ( 1ul, c.chunkCount() );

    c.clear();

    EXPECT_TRUE( c.empty() );
}


TEST( RWConflict, writeBefore)
{
    ReadWriteConflict c;
    EXPECT_TRUE( c.empty() );

    char xs[] = "Hallo, dit is een test";
    //           0123456789012345678901
    //           0000000000111111111122
    //  old block               -----
    //  write block   ----                      

    c.insertRead( xs + 15, 5);
    EXPECT_EQ( 1ul, c.chunkCount() );

    EXPECT_FALSE( c.checkWrite( xs + 5, 4 ) );
    EXPECT_FALSE( c.empty() );

    c.clear();

    EXPECT_TRUE( c.empty() );
}

TEST( RWConflict, writeIsAlreadyCovered)
{
    ReadWriteConflict c;
    EXPECT_TRUE( c.empty() );

    char xs[] = "Hallo, dit is een test";
    //           0123456789012345678901
    //           0000000000111111111122
    //  old block   -----------      
    //  write block     -------                  

    c.insertRead( xs + 3, 11);

    EXPECT_TRUE( c.checkWrite( xs + 7, 7 ) );
    EXPECT_FALSE( c.empty() );
    EXPECT_EQ( 1ul, c.chunkCount() );

    c.clear();

    EXPECT_TRUE( c.empty() );
}

TEST( RWConflict, writeOverlapsBegin)
{
    ReadWriteConflict c;
    EXPECT_TRUE( c.empty() );

    char xs[] = "Hallo, dit is een test";
    //           0123456789012345678901
    //           0000000000111111111122
    //  old block          ------    
    //  write block     -------                  

    c.insertRead( xs + 10, 6);

    EXPECT_TRUE( c.checkWrite( xs + 7, 7 ) );
    EXPECT_FALSE( c.empty() );
    EXPECT_EQ( 1ul, c.chunkCount() );

    c.clear();

    EXPECT_TRUE( c.empty() );
}

TEST( RWConflict, writeOverlapsEnd)
{
    ReadWriteConflict c;
    EXPECT_TRUE( c.empty() );

    char xs[] = "Hallo, dit is een test";
    //           0123456789012345678901
    //           0000000000111111111122
    //  old block          ------    
    //  write block           -----              

    c.insertRead( xs + 10, 6);

    EXPECT_TRUE( c.checkWrite( xs + 13, 5 ) );
    EXPECT_FALSE( c.empty() );
    EXPECT_EQ( 1ul, c.chunkCount() );

    c.clear();

    EXPECT_TRUE( c.empty() );
}

TEST( RWConflict, writeOverlapsMultipleBegin)
{
    ReadWriteConflict c;
    EXPECT_TRUE( c.empty() );

    char xs[] = "Hallo, dit is een test";
    //           0123456789012345678901
    //           0000000000111111111122
    //  old block     ---    --  ---  -
    //  write block        -------      

    c.insertRead( xs + 5, 3);
    c.insertRead( xs + 12, 2);
    c.insertRead( xs + 16, 3);
    c.insertRead( xs + 21, 1);

    EXPECT_TRUE( c.checkWrite( xs + 10, 7 ) );
    EXPECT_FALSE( c.empty() );
    EXPECT_EQ( 4ul, c.chunkCount() );

    c.clear();

    EXPECT_TRUE( c.empty() );
}

TEST( RWConflict, writeDisjointMultiple)
{
    ReadWriteConflict c;
    EXPECT_TRUE( c.empty() );

    char xs[] = "Hallo, dit is een test";
    //           0123456789012345678901
    //           0000000000111111111122
    //  old block     ---    --  ---  -
    //  write block      ---          

    c.insertRead( xs + 5, 3);
    c.insertRead( xs + 12, 2);
    c.insertRead( xs + 16, 3);
    c.insertRead( xs + 21, 1);

    EXPECT_FALSE( c.checkWrite( xs + 8, 3 ) );
    EXPECT_FALSE( c.empty() );
    EXPECT_EQ( 4ul, c.chunkCount() );

    c.clear();

    EXPECT_TRUE( c.empty() );
}

TEST( RWConflict, writeOverlapsMultipleEnd)
{
    ReadWriteConflict c;
    EXPECT_TRUE( c.empty() );

    char xs[] = "Hallo, dit is een test";
    //           0123456789012345678901
    //           0000000000111111111122
    //  old block     ---    --  ---  -
    //  write block           -------   

    c.insertRead( xs + 5, 3);
    c.insertRead( xs + 12, 2);
    c.insertRead( xs + 16, 3);
    c.insertRead( xs + 21, 1);

    EXPECT_TRUE( c.checkWrite( xs + 13, 7 ) );
    EXPECT_FALSE( c.empty() );
    EXPECT_EQ( 4ul, c.chunkCount() );

    c.clear();

    EXPECT_TRUE( c.empty() );
}

TEST( RWConflict, newIsAlreadyCovered)
{
    ReadWriteConflict c;
    EXPECT_TRUE( c.empty() );

    char xs[] = "Hallo, dit is een test";
    //           0123456789012345678901
    //           0000000000111111111122
    //  old block   -----------      
    //  new block       -------                  

    c.insertRead( xs + 3, 11);
    c.insertRead( xs + 7, 7 ) ;
    EXPECT_EQ( 1ul, c.chunkCount() );

    for (int i = 0; i < 22; ++i)
        EXPECT_EQ( i >= 3 && i < 14 , c.checkWrite(xs +i, 1) );

    EXPECT_FALSE( c.empty() );

    c.clear();

    EXPECT_TRUE( c.empty() );
}

TEST( RWConflict, newOverlapsBegin)
{
    ReadWriteConflict c;
    EXPECT_TRUE( c.empty() );

    char xs[] = "Hallo, dit is een test";
    //           0123456789012345678901
    //           0000000000111111111122
    //  old block          ------    
    //  write block     -------                  

    c.insertRead( xs + 10, 6);
    c.insertRead( xs + 7, 7 ) ;
    EXPECT_EQ( 1ul, c.chunkCount() );

    for (int i = 0; i < 22; ++i)
        EXPECT_EQ( i >= 7 && i < 16 , c.checkWrite(xs +i, 1) );

    EXPECT_FALSE( c.empty() );

    c.clear();

    EXPECT_TRUE( c.empty() );
}

TEST( RWConflict, newOverlapsEnd)
{
    ReadWriteConflict c;
    EXPECT_TRUE( c.empty() );

    char xs[] = "Hallo, dit is een test";
    //           0123456789012345678901
    //           0000000000111111111122
    //  old block          ------    
    //  write block           -----              

    c.insertRead( xs + 10, 6);
    c.insertRead( xs + 13, 5);

    EXPECT_EQ( 1ul, c.chunkCount() );
    for (int i = 0; i < 22; ++i)
        EXPECT_EQ( i >= 10 && i < 18, c.checkWrite(xs +i, 1) );

    EXPECT_FALSE( c.empty() );

    c.clear();

    EXPECT_TRUE( c.empty() );
}

TEST( RWConflict, newOverlapsMultipleBegin)
{
    ReadWriteConflict c;
    EXPECT_TRUE( c.empty() );

    char xs[] = "Hallo, dit is een test";
    //           0123456789012345678901
    //           0000000000111111111122
    //  old block     ---    --  ---  -
    //  write block        -------      
    char ys[] = "     ---  ---------  -";

    c.insertRead( xs + 5, 3);
    c.insertRead( xs + 12, 2);
    c.insertRead( xs + 16, 3);
    c.insertRead( xs + 21, 1);

    EXPECT_EQ( 4ul, c.chunkCount() );
    c.insertRead( xs + 10, 7 ) ;
    EXPECT_EQ( 3ul, c.chunkCount() );

    EXPECT_FALSE( c.empty() );

    for (int i = 0; i < 22; ++i)
        EXPECT_EQ( ys[i] == '-', c.checkWrite(xs +i, 1) );

    c.clear();

    EXPECT_TRUE( c.empty() );
}

TEST( RWConflict, newDisjointMultiple)
{
    ReadWriteConflict c;
    EXPECT_TRUE( c.empty() );

    char xs[] = "Hallo, dit is een test";
    //           0123456789012345678901
    //           0000000000111111111122
    //  old block     ---    --  ---  -
    //  write block      ---          
    char ys[] = "     ------ --  ---  -";

    c.insertRead( xs + 5, 3);
    c.insertRead( xs + 12, 2);
    c.insertRead( xs + 16, 3);
    c.insertRead( xs + 21, 1);

    EXPECT_EQ( 4ul, c.chunkCount() );
    c.insertRead( xs + 8, 3 ) ;
    EXPECT_EQ( 4ul, c.chunkCount() );

    EXPECT_FALSE( c.empty() );
    for (int i = 0; i < 22; ++i)
        EXPECT_EQ( ys[i] == '-', c.checkWrite(xs +i, 1) );

    c.clear();

    EXPECT_TRUE( c.empty() );
}

TEST( RWConflict, newOverlapsMultipleEnd)
{
    ReadWriteConflict c;
    EXPECT_TRUE( c.empty() );

    char xs[] = "Hallo, dit is een test";
    //           0123456789012345678901
    //           0000000000111111111122
    //  old block     ---    --  ---  -
    //  write block           --------   
    char ys[] = "     ---    ----------";

    c.insertRead( xs + 5, 3);
    c.insertRead( xs + 12, 2);
    c.insertRead( xs + 16, 3);
    c.insertRead( xs + 21, 1);
    EXPECT_EQ( 4ul, c.chunkCount() );
    c.insertRead( xs + 13, 8 );
    EXPECT_EQ( 2ul, c.chunkCount() );
    EXPECT_FALSE( c.empty() );

    EXPECT_FALSE( c.empty() );
    for (int i = 0; i < 22; ++i)
        EXPECT_EQ( ys[i] == '-', c.checkWrite(xs +i, 1) );

    c.clear();

    EXPECT_TRUE( c.empty() );
}

TEST( RWConflict, lastAddMergesWithEnd)
{
    ReadWriteConflict c;
    EXPECT_TRUE( c.empty() );

    char xs[] = "Hallo, dit is een test";
    //           0123456789012345678901
    //           0000000000111111111122
    //  old block              -----   
    //  write block       -----          
    char ys[] = "         ----------   ";

    c.insertRead( xs + 14, 5);
    EXPECT_EQ( 1ul, c.chunkCount() );
    c.insertRead( xs + 9, 5);
    EXPECT_EQ( 1ul, c.chunkCount() );

    EXPECT_FALSE( c.empty() );
    for (int i = 0; i < 22; ++i)
        EXPECT_EQ( ys[i] == '-', c.checkWrite(xs +i, 1) );
}

