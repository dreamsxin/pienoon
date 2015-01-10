/*
* Copyright (c) 2014 Google, Inc.
*
* This software is provided 'as-is', without any express or implied
* warranty.  In no event will the authors be held liable for any damages
* arising from the use of this software.
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely, subject to the following restrictions:
* 1. The origin of this software must not be misrepresented; you must not
* claim that you wrote the original software. If you use this software
* in a product, an acknowledgment in the product documentation would be
* appreciated but is not required.
* 2. Altered source versions must be plainly marked as such, and must not be
* misrepresented as being the original software.
* 3. This notice may not be removed or altered from any source distribution.
*/

#include "gtest/gtest.h"
#include "flatbuffers/flatbuffers.h"
#include "impel_engine.h"
#include "impel_init.h"
#include "mathfu/constants.h"
#include "angle.h"
#include "common.h"

using fpl::kPi;
using impel::ImpelEngine;
using impel::Impeller1f;
using impel::ImpelTime;
using impel::ImpelInit;
using impel::ImpellerState1f;
using impel::OvershootImpelInit;
using impel::Settled1f;

class ImpelTests : public ::testing::Test {
protected:
  virtual void SetUp()
  {
    impel::OvershootImpelInit::Register();
    impel::SmoothImpelInit::Register();

    // Create an OvershootImpelInit with reasonable values.
    overshoot_angle_init_.set_modular(true);
    overshoot_angle_init_.set_min(-3.14159265359f);
    overshoot_angle_init_.set_max(3.14159265359f);
    overshoot_angle_init_.set_max_velocity(0.021f);
    overshoot_angle_init_.set_max_delta(3.141f);
    overshoot_angle_init_.at_target().max_difference = 0.087f;
    overshoot_angle_init_.at_target().max_velocity = 0.00059f;
    overshoot_angle_init_.set_accel_per_difference(0.00032f);
    overshoot_angle_init_.set_wrong_direction_multiplier(4.0f);
    overshoot_angle_init_.set_max_delta_time(10);

    // Create an OvershootImpelInit that represents a percent from 0 ~ 100.
    // It does not wrap around.
    overshoot_percent_init_.set_modular(false);
    overshoot_percent_init_.set_min(0.0f);
    overshoot_percent_init_.set_max(100.0f);
    overshoot_percent_init_.set_max_velocity(10.0f);
    overshoot_percent_init_.set_max_delta(50.0f);
    overshoot_percent_init_.at_target().max_difference = 0.087f;
    overshoot_percent_init_.at_target().max_velocity = 0.00059f;
    overshoot_percent_init_.set_accel_per_difference(0.00032f);
    overshoot_percent_init_.set_wrong_direction_multiplier(4.0f);
    overshoot_percent_init_.set_max_delta_time(10);
  }
  virtual void TearDown() {}

protected:
  void InitImpeller(const ImpelInit& init, float start_value,
                    float start_velocity, float target_value,
                    Impeller1f* impeller) {
    ImpellerState1f s;
    s.SetValue(start_value);
    s.SetVelocity(start_velocity);
    s.SetTargetValue(target_value);
    impeller->InitializeWithState(init, &engine_, s);
  }

  void InitOvershootImpeller(Impeller1f* impeller) {
    InitImpeller(overshoot_percent_init_, overshoot_percent_init_.max(),
                 overshoot_percent_init_.max_velocity(),
                 overshoot_percent_init_.max(), impeller);
  }

  void InitOvershootImpellerArray(Impeller1f* impellers, int len) {
    for (int i = 0; i < len; ++i) {
      InitOvershootImpeller(&impellers[i]);
    }
  }

  ImpelTime TimeToSettle(const Impeller1f& impeller, const Settled1f& settled) {
    static const ImpelTime kTimePerFrame = 10;
    static const ImpelTime kMaxTime = 10000;

    ImpelTime time = 0;
    while (time < kMaxTime && !settled.Settled(impeller)) {
      engine_.AdvanceFrame(kTimePerFrame);
      time += kTimePerFrame;
    }
    return time;
  }

  ImpelEngine engine_;
  OvershootImpelInit overshoot_angle_init_;
  OvershootImpelInit overshoot_percent_init_;
};

// Ensure we wrap around from pi to -pi.
TEST_F(ImpelTests, ModularMovement) {
  Impeller1f impeller;
  InitImpeller(overshoot_angle_init_, kPi, 0.001f, -kPi + 1.0f, &impeller);
  engine_.AdvanceFrame(1);

  // We expect the position to go up from +pi since it has positive velocity.
  // Since +pi is the max of the range, we expect the value to wrap down to -pi.
  EXPECT_LE(impeller.Value(), 0.0f);
}

// Ensure the simulation settles on the target in a reasonable amount of time.
TEST_F(ImpelTests, EventuallySettles) {
  Impeller1f impeller;
  InitImpeller(overshoot_angle_init_, 0.0f,
               overshoot_angle_init_.max_velocity(),
               -kPi + 1.0f, &impeller);
  const ImpelTime time_to_settle = TimeToSettle(
      impeller, overshoot_angle_init_.at_target());

  // The simulation should complete in about half a second (time is in ms).
  // Checke that it doesn't finish too quickly nor too slowly.
  EXPECT_GT(time_to_settle, 0);
  EXPECT_LT(time_to_settle, 700);
}

// Ensure the simulation settles when the target is the max bound in a modular
// type. It will oscillate between the max and min bound a lot.
TEST_F(ImpelTests, SettlesOnMax) {
  Impeller1f impeller;
  InitImpeller(overshoot_angle_init_, kPi, overshoot_angle_init_.max_velocity(),
               kPi, &impeller);
  const ImpelTime time_to_settle = TimeToSettle(
      impeller, overshoot_angle_init_.at_target());

  // The simulation should complete in about half a second (time is in ms).
  // Checke that it doesn't finish too quickly nor too slowly.
  EXPECT_GT(time_to_settle, 0);
  EXPECT_LT(time_to_settle, 500);
}

// Ensure the simulation does not exceed the max bound, on constrants that
// do not wrap around.
TEST_F(ImpelTests, StaysWithinBound) {
  Impeller1f impeller;
  InitOvershootImpeller(&impeller);
  engine_.AdvanceFrame(1);

  // Even though we're at the bound and trying to travel beyond the bound,
  // the simulation should clamp our position to the bound.
  EXPECT_EQ(impeller.Value(), overshoot_percent_init_.max());
}

// Open up a hole in the data and then call Defragment() to close it.
TEST_F(ImpelTests, Defragment) {
  Impeller1f impellers[4];
  const int len = static_cast<int>(ARRAYSIZE(impellers));
  for (int hole = 0; hole < len; ++hole) {
    InitOvershootImpellerArray(impellers, len);

    // Invalidate impeller at index 'hole'.
    impellers[hole].Invalidate();
    EXPECT_FALSE(impellers[hole].Valid());

    // Defragment() is called at the start of AdvanceFrame.
    engine_.AdvanceFrame(1);
    EXPECT_FALSE(impellers[hole].Valid());

    // Compare the remaining impellers against each other.
    const int compare = hole == 0 ? 1 : 0;
    EXPECT_TRUE(impellers[compare].Valid());
    for (int i = 0; i < len; ++i) {
      if (i == hole || i == compare)
        continue;

      // All the impellers should be valid and have the same values.
      EXPECT_TRUE(impellers[i].Valid());
      EXPECT_EQ(impellers[i].Value(), impellers[compare].Value());
      EXPECT_EQ(impellers[i].Velocity(), impellers[compare].Velocity());
      EXPECT_EQ(impellers[i].TargetValue(), impellers[compare].TargetValue());
    }
  }
}

// Copy a valid impeller. Ensure original impeller gets invalidated.
TEST_F(ImpelTests, CopyConstructor) {
  Impeller1f orig_impeller;
  InitOvershootImpeller(&orig_impeller);
  EXPECT_TRUE(orig_impeller.Valid());
  const float value = orig_impeller.Value();

  Impeller1f new_impeller(orig_impeller);
  EXPECT_FALSE(orig_impeller.Valid());
  EXPECT_TRUE(new_impeller.Valid());
  EXPECT_EQ(new_impeller.Value(), value);
}

// Copy an invalid impeller.
TEST_F(ImpelTests, CopyConstructorInvalid) {
  Impeller1f invalid_impeller;
  EXPECT_FALSE(invalid_impeller.Valid());

  Impeller1f copy_of_invalid(invalid_impeller);
  EXPECT_FALSE(copy_of_invalid.Valid());
}

TEST_F(ImpelTests, AssignmentOperator) {
  Impeller1f orig_impeller;
  InitOvershootImpeller(&orig_impeller);
  EXPECT_TRUE(orig_impeller.Valid());
  const float value = orig_impeller.Value();

  Impeller1f new_impeller;
  new_impeller = orig_impeller;
  EXPECT_FALSE(orig_impeller.Valid());
  EXPECT_TRUE(new_impeller.Valid());
  EXPECT_EQ(new_impeller.Value(), value);
}

TEST_F(ImpelTests, VectorResize) {
  static const int kStartSize = 4;
  std::vector<Impeller1f> impellers(kStartSize);

  // Create the impellers and ensure that they're valid.
  for (int i = 0; i < kStartSize; ++i) {
    InitOvershootImpeller(&impellers[i]);
    EXPECT_TRUE(impellers[i].Valid());
  }

  // Expand the size of 'impellers'. This should force the array to be
  // reallocated and all impellers in the array to be moved.
  const Impeller1f* orig_address = &impellers[0];
  impellers.resize(kStartSize + 1);
  const Impeller1f* new_address = &impellers[0];
  EXPECT_NE(orig_address, new_address);

  // All the move impellers should still be valid.
  for (int i = 0; i < kStartSize; ++i) {
    InitOvershootImpeller(&impellers[i]);
    EXPECT_TRUE(impellers[i].Valid());
  }
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
