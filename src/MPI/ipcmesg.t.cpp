
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

#include "ipcmesg.hpp"

#include <gtest/gtest.h>

using lpf::mpi::IPCMesg;
using namespace lpf::mpi::ipc;

enum MesgType { One, Two, Three };
enum Prop { A, B, C, D };

TEST(IPCMesg, empty) {
  char buf[80];
  IPCMesg<MesgType> m = newMsg(Two, buf, sizeof(buf));

  EXPECT_EQ(Two, m.type());
}

TEST(IPCMesg, twoNumbers) {

  char buf[80];
  IPCMesg<MesgType> m =
      newMsg(Three, buf, sizeof(buf)).write(A, 5).write(B, 500u);

  EXPECT_EQ(Three, m.type());
  m.rewind();

  int a;
  unsigned b;

  m.read(A, a).read(B, b);
  EXPECT_EQ(5, a);
  EXPECT_EQ(500u, b);
}

TEST(IPCMesg, threeNumbersAndABlob) {
  const char str[] = "Test";
  char strbuf[sizeof(str)];

  char buf[80];
  IPCMesg<MesgType> m = newMsg(One, buf, sizeof(buf))
                            .write(B, 5)
                            .write(C, str, sizeof(str))
                            .write(A, 1234567ul);

  EXPECT_EQ(One, m.type());
  m.rewind();
  EXPECT_EQ(One, m.type());

  int b;
  unsigned long a;

  m.read(B, b).read(C, strbuf, sizeof(strbuf)).read(A, a);

  EXPECT_EQ(5, b);
  EXPECT_EQ(1234567ul, a);
  EXPECT_STREQ("Test", strbuf);
}

TEST(IPCMesg, blobEnding) {
  const char str[] = "Test";
  char strbuf[sizeof(str)];

  char buf[80];
  IPCMesg<MesgType> m = newMsg(One, buf, sizeof(buf))
                            .write(B, 5)
                            .write(A, 1234567ul)
                            .write(C, str, sizeof(str));

  EXPECT_EQ(One, m.type());

  int b;
  unsigned long a;

  IPCMesg<MesgType> m2(buf, m.pos(), 0);
  m2.read(B, b).read(A, a);

  m2.read(C, strbuf, m2.bytesLeft());

  EXPECT_EQ(5, b);
  EXPECT_EQ(1234567ul, a);
  EXPECT_STREQ("Test", strbuf);
}
