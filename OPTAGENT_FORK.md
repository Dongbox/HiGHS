# OptAgent HiGHS fork

This fork starts from upstream v1.15.1 at
`04024d701f79feb8e2f18bc3df0dffc04ef05088`. Original licenses and notices
remain applicable. The upstream version number remains 1.15.1; identify this
fork by repository and commit, not by the version string alone.

## Numerical changes

Binary substitution requires actual row redundancy and endpoint implication.
MIP feasibility tolerance is not subtracted from those requirements. A recovery
map whose rounded scale does not preserve its endpoints is skipped.

The added `mip_strict_numerics` option defaults to false for upstream behavioral
compatibility. When enabled, MIP presolve and LP relaxation primal precision use
`min(mip_feasibility_tolerance, 1e-10)`. Candidate acceptance retains the
requested MIP tolerance; it is a separate contract. LP dual tolerance retains
upstream policy. OptAgent enables this mode and independently verifies original
model semantics before publishing a candidate.

Strict numerical precision may require more work and is not a guarantee that
arbitrary ill-conditioned models can be solved. Do not switch the standalone
default without checking the upstream MIP regressions, including issue-2122.

## Maintenance and validation

Develop changes on topic branches, keep upstream changes separate, and pin
OptAgent to a tested full commit. Never move an already consumed source tag.
All builds must use headers and libraries from the same fork revision.

```sh
cmake -S . -B build/optagent-root-fix -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=17 \
  -DALL_TESTS=ON -DBUILD_TESTING=ON -DBUILD_CXX_EXE=ON
cmake --build build/optagent-root-fix --target unit_tests
build/optagent-root-fix/bin/unit_tests \
  '[highs_test_presolve_rules],[highs_test_presolve],[highs_test_mip_solver]'
```

The narrow-band regression exercises both bound directions, four MIP tolerances,
and both numerical modes. Strict mode also checks primal residuals against
1e-10. OptAgent owns expression-graph verification and optimality certification;
those guarantees do not come from this fork's raw `Optimal` status alone.
