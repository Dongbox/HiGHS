#include <numeric>

#include "HCheckConfig.h"
#include "Highs.h"
#include "catch.hpp"

const bool dev_run = false;

TEST_CASE("binary-substitution-preserves-narrow-feasible-band",
          "[highs_test_presolve_rules][optagent_regression]") {
  const bool strict = GENERATE(false, true);
  Highs::resetGlobalScheduler(true);
  // All zeros are a feasible integer witness. At 1e-6, tolerant binary
  // substitution used to replace a by 20.000001 - 20.000002*z, force z=1,
  // and manufacture an infeasible continuous band before any LP was solved.
  // Reflecting the continuous variables exercises both bound directions.
  for (const bool reflected : {false, true}) {
    for (const double tolerance : {1e-10, 1e-7, 1e-6, 1e-5}) {
      CAPTURE(reflected, tolerance, strict);
      HighsLp lp;
      lp.num_col_ = 5;
      lp.num_row_ = 4;
      lp.col_cost_.assign(5, 0.0);
      lp.col_lower_ = {-1e-6, -1e-6, 0, -1e-6, -1e-6};
      lp.col_upper_ = {20.000001, 100.000001, 1, 20.000001, 20.000001};
      lp.integrality_.assign(5, HighsVarType::kContinuous);
      lp.integrality_[2] = HighsVarType::kInteger;
      lp.row_lower_ = {-1e-7, -kHighsInf, -1e-7, -kHighsInf};
      lp.row_upper_ = {kHighsInf, 1e-7, kHighsInf, 20};
      lp.a_matrix_.format_ = MatrixFormat::kRowwise;
      lp.a_matrix_.start_ = {0, 3, 6, 9, 11};
      lp.a_matrix_.index_ = {0, 3, 4, 1, 3, 4, 1, 3, 4, 0, 2};
      lp.a_matrix_.value_ = {1, -1, -1, 1, -5, -5, 1, -5, -5, 1, 20};
      if (reflected) {
        for (HighsInt col : {0, 1, 3, 4}) {
          const double lower = lp.col_lower_[col];
          lp.col_lower_[col] = -lp.col_upper_[col];
          lp.col_upper_[col] = -lower;
        }
        for (HighsInt nz = 0; nz < 10; ++nz)
          lp.a_matrix_.value_[nz] = -lp.a_matrix_.value_[nz];
      }
      Highs highs;
      REQUIRE(highs.setOptionValue("output_flag", false) == HighsStatus::kOk);
      REQUIRE(highs.setOptionValue("threads", 1) == HighsStatus::kOk);
      REQUIRE(highs.setOptionValue("presolve", "on") == HighsStatus::kOk);
      REQUIRE(highs.setOptionValue("mip_strict_numerics", strict) ==
              HighsStatus::kOk);
      REQUIRE(highs.setOptionValue("mip_feasibility_tolerance", tolerance) ==
              HighsStatus::kOk);
      REQUIRE(highs.passModel(lp) == HighsStatus::kOk);
      REQUIRE(highs.run() == HighsStatus::kOk);
      REQUIRE(highs.getModelStatus() == HighsModelStatus::kOptimal);
      REQUIRE(highs.getSolution().value_valid);
      if (strict) REQUIRE(highs.getInfo().max_primal_infeasibility <= 1e-10);
    }
  }
  Highs::resetGlobalScheduler(true);
}

void presolveOffOn(const std::string& message, const HighsLp& lp, Highs& h,
                   const HighsInt require_presolved_model_num_col = 0,
                   const HighsInt require_presolved_model_num_row = 0,
                   const HighsInt require_presolved_model_num_nz = 0);

TEST_CASE("test-col-stuffing", "[highs_test_presolve_rules]") {
  HighsLp lp;

  Highs h;
  h.setOptionValue("output_flag", dev_run);
  h.setOptionValue("presolve_rule_test", kPresolveRuleColStuffing);
  const bool lp0 = true;
  const bool lp1 = true;
  const bool lp1a = true;
  const bool lp1b = true;

  if (lp0) {
    lp.num_col_ = 3;
    lp.num_row_ = 1;
    lp.sense_ = ObjSense::kMaximize;
    lp.col_cost_ = {1.8, 0.9, 1};
    lp.col_lower_.assign(lp.num_col_, 0);
    lp.col_upper_.assign(lp.num_col_, 1);
    lp.row_lower_ = {-kHighsInf};
    lp.row_upper_ = {4};
    lp.a_matrix_.format_ = MatrixFormat::kRowwise;
    lp.a_matrix_.start_ = {0, lp.num_col_};
    lp.a_matrix_.index_.resize(lp.num_col_);
    std::iota(lp.a_matrix_.index_.begin(), lp.a_matrix_.index_.end(), 0);
    lp.a_matrix_.value_ = {3, 2, 2};

    for (int k = 0; k < 2; k++) {
      if (dev_run) printf("\n3-variable knapsack: %s\n", k == 0 ? "LP" : "IP");
      REQUIRE(h.passModel(lp) == HighsStatus::kOk);
      h.setOptionValue("presolve_rule_test", kPresolveRuleColStuffing);
      h.run();
      if (dev_run) h.writeSolution("", 1);
      lp.integrality_.assign(lp.num_col_, HighsVarType::kInteger);
    }
    lp.clear();
  }

  lp.num_col_ = 2;
  lp.num_row_ = 1;
  lp.sense_ = ObjSense::kMinimize;
  lp.col_lower_.assign(lp.num_col_, 0);
  lp.col_upper_.assign(lp.num_col_, 1);
  lp.row_lower_ = {2.0};
  lp.row_upper_ = {kHighsInf};
  lp.a_matrix_.format_ = MatrixFormat::kRowwise;
  lp.a_matrix_.start_ = {0, lp.num_col_};
  lp.a_matrix_.index_.resize(lp.num_col_);
  std::iota(lp.a_matrix_.index_.begin(), lp.a_matrix_.index_.end(), 0);
  if (lp1) {
    lp.col_cost_.assign(lp.num_col_, 1);
    lp.a_matrix_.value_.assign(lp.num_col_, 1);
    presolveOffOn("Capturing neos-787933 issue", lp, h);
  }
  if (lp1a) {
    lp.col_cost_ = {2, 1};
    lp.a_matrix_.value_.assign(lp.num_col_, 1);
    presolveOffOn("Variant A neos-787933 issue", lp, h);
  }
  if (lp1b) {
    lp.col_cost_ = {-2, -1};
    lp.a_matrix_.value_.assign(lp.num_col_, 1);
    presolveOffOn("Variant B neos-787933 issue", lp, h);
  }
  lp.clear();

  h.resetGlobalScheduler(true);
}

void presolveOffOn(const std::string& message, const HighsLp& lp, Highs& h,
                   const HighsInt require_presolved_model_num_col,
                   const HighsInt require_presolved_model_num_row,
                   const HighsInt require_presolved_model_num_nz) {
  const HighsRunData& run_data = h.getRunData();
  bool presolve_on = false;
  // If the model reduces to empty, then the output from different
  // solvers cannot be tested
  const bool reduce_to_empty = require_presolved_model_num_col == 0 &&
                               require_presolved_model_num_row == 0;
  const HighsInt to_k = reduce_to_empty ? 2 : 5;
  for (int k = 0; k < to_k; k++) {
    std::string solver = kSimplexString;
    std::string run_crossover = kHighsOnString;
    bool basis_postsolve = true;
    if (k == 0) {
      // Presolve off - to get the optimal solution to debug
      // presolve
      presolve_on = false;
    } else {
      presolve_on = true;
      if (k == 1) {
        solver = kSimplexString;
      } else if (k == 2) {
        solver = kIpmString;
      } else if (k == 3) {
        solver = kIpmString;
        run_crossover = kHighsOffString;
        basis_postsolve = false;
      } else {
        solver = kHiPdlpString;
        basis_postsolve = false;
      }
    }
    std::string presolve = presolve_on ? kHighsOnString : kHighsOffString;
    h.setOptionValue(kPresolveString, presolve);
    h.setOptionValue(kRunCrossoverString, run_crossover);
    h.setOptionValue(kSolverString, solver);
    if (dev_run)
      printf(
          "\n============\n%s: presolve = %s; solver = %s%s\n============\n\n",
          message.c_str(), presolve.c_str(), solver.c_str(),
          solver == kIpmString ? ("; run_crossover = " + run_crossover).c_str()
                               : "");
    REQUIRE(h.passModel(lp) == HighsStatus::kOk);
    h.run();
    if (dev_run) h.writeSolution("", 1);
    if (presolve_on) {
      // Ensure that the model is reduced to empty
      REQUIRE(run_data.presolved_model_num_col ==
              require_presolved_model_num_col);
      REQUIRE(run_data.presolved_model_num_row ==
              require_presolved_model_num_row);
      REQUIRE(run_data.presolved_model_num_nz ==
              require_presolved_model_num_nz);
      // Ensure that dual postsolve is correct
      REQUIRE(h.getModelStatus() == HighsModelStatus::kOptimal);
      REQUIRE(h.getInfo().num_primal_infeasibilities == 0);
      REQUIRE(h.getInfo().num_dual_infeasibilities == 0);
      REQUIRE(h.getInfo().simplex_iteration_count == 0);
      // Ensure that any basis postsolve is correct
      if (basis_postsolve)
        REQUIRE(run_data.num_simplex_iterations_after_postsolve == 0);
    }
  }
}
