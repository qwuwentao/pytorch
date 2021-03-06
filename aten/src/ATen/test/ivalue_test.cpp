#include <ATen/ATen.h>
#include <gtest/gtest.h>
#include <torch/torch.h>
#include <c10/util/intrusive_ptr.h>

namespace c10 {

TEST(IValueTest, Basic) {
  c10::List<int64_t> foo({3, 4, 5});
  ASSERT_EQ(foo.use_count(), 1);
  IValue bar{foo};
  ASSERT_EQ(foo.use_count(), 2);
  auto baz = bar;
  ASSERT_EQ(foo.use_count(), 3);
  auto foo2 = std::move(bar);
  ASSERT_EQ(foo.use_count(), 3);
  ASSERT_TRUE(foo2.isIntList());
  ASSERT_TRUE(bar.isNone());
  foo2 = IValue(4.0);
  ASSERT_TRUE(foo2.isDouble());
  ASSERT_EQ(foo2.toDouble(), 4.0);
  ASSERT_EQ(foo.use_count(), 2);
  ASSERT_TRUE(baz.toIntVector() == std::vector<int64_t>({3, 4, 5}));

  auto move_it = std::move(baz).toIntList();
  ASSERT_EQ(foo.use_count(), 2);
  ASSERT_TRUE(baz.isNone());
  IValue i(4);
  ASSERT_TRUE(i.isInt());
  ASSERT_EQ(i.toInt(), 4);
  IValue dlist(c10::List<double>({3.5}));
  ASSERT_TRUE(dlist.isDoubleList());
  ASSERT_TRUE(dlist.toDoubleVector() == std::vector<double>({3.5}));
  std::move(dlist).toDoubleList();
  ASSERT_TRUE(dlist.isNone());
  dlist = IValue(c10::List<double>({3.4}));
  ASSERT_TRUE(dlist.toDoubleVector() == std::vector<double>({3.4}));
  IValue the_list(
      at::ivalue::Tuple::create({IValue(3.4), IValue(4), IValue(foo)}));
  ASSERT_EQ(foo.use_count(), 3);
  ASSERT_TRUE(the_list.isTuple());
  auto first = the_list.toTuple()->elements()[1];
  ASSERT_EQ(first.toInt(), 4);
  at::Tensor tv = at::rand({3, 4});
  IValue ten(tv);
  ASSERT_EQ(tv.use_count(), 2);
  auto ten2 = ten;
  ASSERT_EQ(tv.use_count(), 3);
  ASSERT_TRUE(ten2.toTensor().equal(ten.toTensor()));
  std::move(ten2).toTensor();
  ASSERT_EQ(tv.use_count(), 2);
}

static std::array<IValue, 5> makeSampleIValues() {
  return { at::rand({3, 4}), "hello", 42, true, 1.5 };
}

static std::array<IValue, 5> makeMoreSampleIValues() {
  return { at::rand({3, 4}), "goodbye", 23, false, 0.5 };
}

// IValue::operator== doesn't seem to work on Tensors.
#define EXPECT_IVALUE_EQ(a, b)                          \
  EXPECT_EQ((a).isTensor(), (b).isTensor());            \
  if ((a).isTensor()) {                                 \
    EXPECT_TRUE(a.toTensor().equal(b.toTensor()));      \
  } else {                                              \
    EXPECT_EQ(a, b);                                    \
  }

TEST(IValueTest, Swap) {
  // swap() has the following 3 cases: tensor, intrusive_ptr, or
  // neither. Exercise all pairs of the three.

  auto sampleInputs = makeSampleIValues();
  auto sampleTargets = makeMoreSampleIValues();
  for (const auto& input: sampleInputs) {
    for (const auto& target: sampleTargets) {
      IValue a(input);
      IValue b(target);
      EXPECT_IVALUE_EQ(a, input);
      EXPECT_IVALUE_EQ(b, target);
      a.swap(b);
      EXPECT_IVALUE_EQ(a, target);
      EXPECT_IVALUE_EQ(b, input);
    }
  }
}

TEST(IValueTest, CopyConstruct) {
  auto sampleInputs = makeSampleIValues();
  for (const IValue& v: sampleInputs) {
    IValue copy(v);
    EXPECT_IVALUE_EQ(copy, v);
  }
}

TEST(IValueTest, MoveConstruct) {
  auto sampleInputs = makeSampleIValues();
  for (const IValue& v: sampleInputs) {
    IValue source(v);
    IValue target(std::move(source));
    EXPECT_IVALUE_EQ(target, v);
    EXPECT_TRUE(source.isNone());
  }
}

TEST(IValueTest, CopyAssign) {
  auto sampleInputs = makeSampleIValues();
  auto sampleTargets = makeMoreSampleIValues();

  for (const IValue& input: sampleInputs) {
    for (const IValue& target: sampleTargets) {
      IValue copyTo(target);
      IValue copyFrom(input);
      copyTo = copyFrom;
      EXPECT_IVALUE_EQ(copyTo, input);
      EXPECT_IVALUE_EQ(copyFrom, input);
      EXPECT_IVALUE_EQ(copyTo, copyFrom);
    }
  }
}

TEST(IValueTest, MoveAssign) {
  auto sampleInputs = makeSampleIValues();
  auto sampleTargets = makeMoreSampleIValues();

  for (const IValue& input: sampleInputs) {
    for (const IValue& target: sampleTargets) {
      IValue moveTo(target);
      IValue moveFrom(input);
      moveTo = std::move(moveFrom);
      EXPECT_IVALUE_EQ(moveTo, input);
      EXPECT_TRUE(moveFrom.isNone());
    }
  }
}

TEST(IValueTest, Tuple) {
  std::tuple<int64_t, at::Tensor> t = std::make_tuple(123, at::randn({1}));
  auto iv = IValue(t);
  auto t_ = iv.to<std::tuple<int64_t, at::Tensor>>();
  ASSERT_EQ(std::get<0>(t_), 123);
  ASSERT_EQ(
      std::get<1>(t_).item().to<float>(), std::get<1>(t).item().to<float>());
}

TEST(IValueTest, unsafeRemoveAttr) {
  auto cu = std::make_shared<CompilationUnit>();
  auto cls = ClassType::create("foo.bar", cu);
  cls->addAttribute("attr1", TensorType::get());
  cls->addAttribute("attr2", TensorType::get());
  auto obj = c10::ivalue::Object::create(
      c10::StrongTypePtr(cu, cls), cls->numAttributes());
  obj->unsafeRemoveAttr("attr1");
  // attr1 is not removed in the type
  ASSERT_TRUE(cls->hasAttribute("attr1"));
  ASSERT_TRUE(cls->hasAttribute("attr2"));
  ASSERT_TRUE(obj->slots().size() == 1);
}

TEST(IValueTest, TuplePrint) {
  {
    IValue tp = std::make_tuple(3);

    std::stringstream ss;
    ss << tp;
    ASSERT_EQ(ss.str(), "(3,)");
  }

  {
    IValue tp = std::make_tuple(3, 3);
    std::stringstream ss;
    ss << tp;
    ASSERT_EQ(ss.str(), "(3, 3)");
  }
}

TEST(IValueTest, BasicFuture) {
  auto f1 = c10::make_intrusive<ivalue::Future>(IntType::get());
  ASSERT_FALSE(f1->completed());

  f1->markCompleted(IValue(42));
  ASSERT_TRUE(f1->completed());
  ASSERT_EQ(42, f1->value().toInt());
  IValue iv(f1);
  ASSERT_EQ(42, iv.toFuture()->value().toInt());
}

TEST(IValueTest, FutureCallbacks) {
  auto f2 = c10::make_intrusive<ivalue::Future>(IntType::get());
  int calledTimesA = 0;
  int calledTimesB = 0;
  f2->addCallback([f2, &calledTimesA]() {
    ASSERT_TRUE(f2->completed());
    ASSERT_EQ(f2->value().toInt(), 43);
    ++calledTimesA;
  });
  f2->markCompleted(IValue(43));
  ASSERT_EQ(calledTimesA, 1);
  ASSERT_EQ(calledTimesB, 0);
  // Post-markCompleted()
  f2->addCallback([f2, &calledTimesB]() {
    ASSERT_TRUE(f2->completed());
    ASSERT_EQ(f2->value().toInt(), 43);
    ++calledTimesB;
  });
  ASSERT_EQ(calledTimesA, 1);
  ASSERT_EQ(calledTimesB, 1);
  ASSERT_FALSE(f2->hasError());
}

TEST(IValueTest, FutureExceptions) {
  auto f3 = c10::make_intrusive<ivalue::Future>(IntType::get());
  int calledTimes = 0;
  f3->addCallback([f3, &calledTimes]() {
    ASSERT_TRUE(f3->completed());
    try {
      (void)f3->value();
    } catch (const std::exception& e) {
      if (std::string(e.what()) == "My Error") {
        ++calledTimes;
      }
    }
  });
  ivalue::Future::FutureError err("My Error");
  f3->setError(std::make_exception_ptr(err));
  ASSERT_EQ(calledTimes, 1);
  ASSERT_TRUE(f3->hasError());
  ASSERT_EQ(f3->tryRetrieveErrorMessage(), std::string("My Error"));
}

TEST(IValueTest, ValueEquality) {
  EXPECT_EQ(IValue("asdf"), IValue("asdf"));
  EXPECT_NE(IValue("asdf"), IValue("ASDF"));
  EXPECT_NE(IValue("2"), IValue(2));
  EXPECT_EQ(IValue(1), IValue(1));

  // Check the equals() variant that returns an IValue
  auto res = IValue("asdf").equals("asdf");
  EXPECT_TRUE(res.isBool());
  EXPECT_TRUE(res.toBool());

  res = IValue("asdf").equals(1);
  EXPECT_TRUE(res.isBool());
  EXPECT_FALSE(res.toBool());
}

TEST(IValueTest, TensorEquality) {
  auto rawTensor = torch::zeros({2, 3});
  auto rawTensorCopy = rawTensor.clone();
  auto t = IValue(rawTensor);
  auto tCopy = IValue(rawTensorCopy);

  // This should throw, because elementwise equality is ambiguous for
  // multi-element Tensors.
  auto testEquality = []() {
    return IValue(torch::ones({2, 3})) == IValue(torch::rand({2, 3}));
  };
  EXPECT_ANY_THROW(testEquality());

  // equals() should return a tensor of all `true`.
  IValue eqTensor = t.equals(tCopy);
  EXPECT_TRUE(eqTensor.isTensor());
  auto booleanTrue = torch::ones({2, 3}).to(torch::kBool);
  EXPECT_TRUE(eqTensor.toTensor().equal(booleanTrue));

  // Test identity checking
  EXPECT_TRUE(t.is(t));
  EXPECT_FALSE(t.is(tCopy));
  IValue tReference = t;
  EXPECT_TRUE(t.is(tReference));
}

TEST(IValueTest, ListEquality) {
  IValue c1 = std::vector<int64_t>{0, 1, 2, 3};
  IValue c2 = std::vector<int64_t>{0, 1, 2, 3};
  IValue c3 = std::vector<int64_t>{0, 1, 2, 3, 4};
  EXPECT_EQ(c1, c1);
  EXPECT_EQ(c1, c2);
  EXPECT_FALSE(c1.is(c2));
  EXPECT_NE(c1, c3);
  EXPECT_NE(c2, c3);
}

TEST(IValueTest, DictEquality) {
  auto innerDict = c10::Dict<std::string, std::string>();
  innerDict.insert("foo", "bar");

  auto d1 = c10::Dict<std::string, c10::Dict<std::string, std::string>>();
  d1.insert("one", innerDict);
  d1.insert("two", innerDict);
  d1.insert("three", innerDict);
  auto c1 = IValue(d1);

  auto d2 = c10::Dict<std::string, c10::Dict<std::string, std::string>>();
  d2.insert("one", innerDict.copy());
  d2.insert("two", innerDict.copy());
  d2.insert("three", innerDict.copy());
  auto c2 = IValue(d2);

  auto d3 = c10::Dict<std::string, c10::Dict<std::string, std::string>>();
  d3.insert("one", innerDict.copy());
  d3.insert("two", innerDict.copy());
  d3.insert("three", innerDict.copy());
  d3.insert("four", innerDict.copy());
  auto c3 = IValue(d3);

  auto d4 = c10::Dict<std::string, c10::Dict<std::string, std::string>>();
  d4.insert("one", innerDict.copy());
  d4.insert("two", innerDict.copy());
  auto innerDictNotEqual = c10::Dict<std::string, std::string>();
  innerDictNotEqual.insert("bar", "foo");
  d4.insert("three", innerDictNotEqual);
  auto c4 = IValue(d4);

  EXPECT_EQ(c1, c1);
  EXPECT_EQ(c1, c2);
  EXPECT_FALSE(c1.is(c2));
  EXPECT_NE(c1, c3);
  EXPECT_NE(c2, c3);
  EXPECT_NE(c1, c4);
  EXPECT_NE(c2, c4);
}

TEST(IValueTest, DictEqualityDifferentOrder) {
  auto d1 = c10::Dict<std::string, int64_t>();
  d1.insert("one", 1);
  d1.insert("two", 2);
  auto d2 = c10::Dict<std::string, int64_t>();
  d2.insert("two", 2);
  d2.insert("one", 1);

  EXPECT_EQ(d1, d2);
}

TEST(IValueTest, ListNestedEquality) {
  IValue c1 = std::vector<std::vector<int64_t>>({{0}, {0, 1}, {0, 1, 2}});
  IValue c2 = std::vector<std::vector<int64_t>>({{0}, {0, 1}, {0, 1, 2}});
  IValue c3 = std::vector<std::vector<int64_t>>({{1}, {0, 1}, {0, 1, 2}});
  EXPECT_EQ(c1, c1);
  EXPECT_EQ(c1, c2);
  EXPECT_NE(c1, c3);
  EXPECT_NE(c2, c3);
}

TEST(IValueTest, StreamEquality) {
  at::Device device1 =  at::Device(kCUDA, 0);
  at::Device device2 = at::Device(kCUDA, 1);
  c10::Stream stream1 = c10::Stream(c10::Stream::Default::DEFAULT, device1);
  c10::Stream stream2 = c10::Stream(c10::Stream::Default::DEFAULT, device2);
  IValue lhs(stream1);
  IValue rhs_different(stream2);
  IValue rhs_same(stream1);
  EXPECT_FALSE(lhs.equals(rhs_different).toBool());
  EXPECT_TRUE(lhs.equals(rhs_same).toBool());
}

TEST(IValueTest, EnumEquality) {
  auto cu = std::make_shared<CompilationUnit>();
  IValue int_ivalue_1(1);
  IValue int_ivalue_2(2);
  IValue str_ivalue_1("1");
  auto int_enum_type1 = EnumType::create(
      "enum_class_1",
      IntType::get(),
      {{"enum_name_1", int_ivalue_1}, {"enum_name_2", int_ivalue_2}},
      cu);
  auto int_enum_type2 = EnumType::create(
      "enum_class_2",
      IntType::get(),
      {{"enum_name_1", int_ivalue_1}, {"enum_name_2", int_ivalue_2}},
      cu);
  auto string_enum_type = EnumType::create(
      "enum_class_3", StringType::get(), {{"enum_name_1", str_ivalue_1}}, cu);

  EXPECT_EQ(
      IValue(c10::make_intrusive<ivalue::EnumHolder>(
          int_enum_type1, "enum_name_1", int_ivalue_1)),
      IValue(c10::make_intrusive<ivalue::EnumHolder>(
          int_enum_type1, "enum_name_1", int_ivalue_1))
  );

  EXPECT_NE(
      IValue(c10::make_intrusive<ivalue::EnumHolder>(
          int_enum_type1, "enum_name_1", int_ivalue_1)),
      IValue(c10::make_intrusive<ivalue::EnumHolder>(
          int_enum_type2, "enum_name_1", int_ivalue_1))
  );

  EXPECT_NE(
      IValue(c10::make_intrusive<ivalue::EnumHolder>(
          int_enum_type1, "enum_name_1", int_ivalue_1)),
      IValue(c10::make_intrusive<ivalue::EnumHolder>(
          int_enum_type1, "enum_name_2", int_ivalue_2))
  );

  EXPECT_NE(
      IValue(c10::make_intrusive<ivalue::EnumHolder>(
          int_enum_type1, "enum_name_1", int_ivalue_1)),
      IValue(c10::make_intrusive<ivalue::EnumHolder>(
          string_enum_type, "enum_name_1", str_ivalue_1))
  );
}

TEST(IValueTest, isPtrType) {
  IValue tensor(at::rand({3, 4}));
  IValue undefinedTensor((at::Tensor()));
  IValue integer(42);
  IValue str("hello");

  EXPECT_TRUE(tensor.isPtrType());
  EXPECT_FALSE(undefinedTensor.isPtrType());
  EXPECT_FALSE(integer.isPtrType());
  EXPECT_TRUE(str.isPtrType());
}

TEST(IValueTest, isAliasOf) {
  auto sampleIValues = makeSampleIValues();
  for (auto& iv: sampleIValues) {
    for (auto& iv2: sampleIValues) {
      if (&iv == &iv2 && iv.isPtrType()) {
        EXPECT_TRUE(iv.isAliasOf(iv2));
      } else {
        EXPECT_FALSE(iv.isAliasOf(iv2));
      }
    }
  }
}

TEST(IValueTest, internalToPointer) {
  IValue tensor(at::rand({3, 4}));
  IValue str("hello");

  EXPECT_EQ(tensor.internalToPointer(), tensor.unsafeToTensorImpl());
  EXPECT_NE(str.internalToPointer(), nullptr);

  IValue nullStr((c10::intrusive_ptr<ivalue::ConstantString>()));
  ASSERT_TRUE(nullStr.isString());
  EXPECT_EQ(nullStr.internalToPointer(), nullptr);
}

TEST(IValueTest, IdentityComparisonAndHashing) {
  at::Tensor t1 = at::rand({3, 4});
  at::Tensor t2 = at::rand({3, 4});
  IValue tv1(t1), tv2(t2);
  IValue tv1b(t1);

  EXPECT_EQ(tv1.hash(), tv1b.hash());
  EXPECT_NE(tv1.hash(), tv2.hash());

  EXPECT_TRUE(tv1.is(tv1));
  EXPECT_TRUE(tv1.is(tv1b));
  EXPECT_TRUE(tv1b.is(tv1));
  EXPECT_TRUE(tv2.is(tv2));

  EXPECT_FALSE(tv1.is(tv2));
  EXPECT_FALSE(tv2.is(tv1));

  IValue none;
  IValue undefinedTensor((at::Tensor()));

  EXPECT_TRUE(none.is(undefinedTensor));
  EXPECT_TRUE(undefinedTensor.is(none));

  // Is this a bug? We should probably have a is b => a.hash() == b.hash()
  EXPECT_NE(none.hash(), undefinedTensor.hash());

  auto sampleIValues = makeSampleIValues();
  auto sampleIValues2 = makeSampleIValues();
  auto moreSampleIValues = makeMoreSampleIValues();

  ASSERT_EQ(sampleIValues.size(), moreSampleIValues.size());
  for (int ii = 0; ii < sampleIValues.size(); ++ii) {
    // Constant strings will have the same pointer value.
    if (sampleIValues[ii].isPtrType() && !sampleIValues[ii].isString()) {
      EXPECT_NE(sampleIValues[ii].hash(), sampleIValues2[ii].hash());
    } else {
      EXPECT_EQ(sampleIValues[ii].hash(), sampleIValues2[ii].hash());
    }
    EXPECT_NE(sampleIValues[ii].hash(), moreSampleIValues[ii].hash());
  }
}

TEST(IValueTest, getSubValues) {
  // Scalars have no subvalues.
  IValue integer(42), float_(1.5);

  IValue::HashAliasedIValues subvalues;

  integer.getSubValues(subvalues);
  EXPECT_TRUE(subvalues.empty());

  subvalues.clear();

  float_.getSubValues(subvalues);
  EXPECT_TRUE(subvalues.empty());

  subvalues.clear();

  at::Tensor t1(at::rand({3, 4})), t2(at::rand({3, 4}));
  IValue tv1(t1), tv2(t2);
  IValue list(std::vector<at::Tensor>{t1, t2});
  IValue tuple(ivalue::Tuple::create({tv1, tv2}));

  std::unordered_map<int64_t, at::Tensor> m;
  m[1] = t1;
  m[2] = t2;

  IValue dict(std::move(m));

  auto objType = ClassType::create(nullopt, {});
  objType->addAttribute("t1", tv1.type());
  objType->addAttribute("t2", tv2.type());

  auto o = ivalue::Object::create(StrongTypePtr(nullptr, objType), 2);
  o->setSlot(0, tv1);
  o->setSlot(1, tv2);

  IValue object(o);
  tv1.getSubValues(subvalues);
  EXPECT_EQ(subvalues.size(), 1);
  EXPECT_EQ(subvalues.count(tv1), 1);

  subvalues.clear();

  for (auto& container: {list, tuple, dict, object}) {
    container.getSubValues(subvalues);
    EXPECT_EQ(subvalues.size(), 3);
    EXPECT_EQ(subvalues.count(container), 1);
    EXPECT_EQ(subvalues.count(tv1), 1);
    EXPECT_EQ(subvalues.count(tv2), 1);

    subvalues.clear();
  }
}

// TODO(gmagogsfm): Add type conversion test?
} // namespace c10
