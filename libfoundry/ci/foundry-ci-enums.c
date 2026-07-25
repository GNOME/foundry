/* foundry-ci-enums.c
 *
 * Copyright 2026 Christian Hergert
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "foundry-ci-enums.h"

/**
 * FoundryCiArtifactKind:
 * @FOUNDRY_CI_ARTIFACT_KIND_FILE: a regular file
 * @FOUNDRY_CI_ARTIFACT_KIND_DIRECTORY: a directory
 * @FOUNDRY_CI_ARTIFACT_KIND_JUNIT: a JUnit test report
 * @FOUNDRY_CI_ARTIFACT_KIND_COVERAGE: a coverage report
 * @FOUNDRY_CI_ARTIFACT_KIND_CODE_QUALITY: a code quality report
 *
 * Describes the kind of output produced by a CI run.
 *
 * Since: 1.2
 */
G_DEFINE_ENUM_TYPE (FoundryCiArtifactKind, foundry_ci_artifact_kind,
                    G_DEFINE_ENUM_VALUE (FOUNDRY_CI_ARTIFACT_KIND_FILE, "file"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_CI_ARTIFACT_KIND_DIRECTORY, "directory"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_CI_ARTIFACT_KIND_JUNIT, "junit"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_CI_ARTIFACT_KIND_COVERAGE, "coverage"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_CI_ARTIFACT_KIND_CODE_QUALITY, "code-quality"))

/**
 * FoundryCiJobDisposition:
 * @FOUNDRY_CI_JOB_DISPOSITION_SELECTED: the job is selected for execution
 * @FOUNDRY_CI_JOB_DISPOSITION_SKIPPED: the job was skipped by pipeline rules
 * @FOUNDRY_CI_JOB_DISPOSITION_MANUAL: the job requires manual selection
 * @FOUNDRY_CI_JOB_DISPOSITION_UNSUPPORTED: the job cannot run locally
 * @FOUNDRY_CI_JOB_DISPOSITION_BLOCKED: the job is blocked by dependencies
 *
 * Describes how a CI provider selected or excluded a job.
 *
 * Since: 1.2
 */
G_DEFINE_ENUM_TYPE (FoundryCiJobDisposition, foundry_ci_job_disposition,
                    G_DEFINE_ENUM_VALUE (FOUNDRY_CI_JOB_DISPOSITION_SELECTED, "selected"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_CI_JOB_DISPOSITION_SKIPPED, "skipped"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_CI_JOB_DISPOSITION_MANUAL, "manual"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_CI_JOB_DISPOSITION_UNSUPPORTED, "unsupported"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_CI_JOB_DISPOSITION_BLOCKED, "blocked"))

/**
 * FoundryCiRunState:
 * @FOUNDRY_CI_RUN_STATE_PENDING: the run has not started
 * @FOUNDRY_CI_RUN_STATE_PREPARING: the run is preparing its environment
 * @FOUNDRY_CI_RUN_STATE_RUNNING: the run is executing
 * @FOUNDRY_CI_RUN_STATE_PASSED: the run completed successfully
 * @FOUNDRY_CI_RUN_STATE_FAILED: the run failed
 * @FOUNDRY_CI_RUN_STATE_CANCELLED: the run was cancelled
 *
 * Describes the current state of a CI run.
 *
 * Since: 1.2
 */
G_DEFINE_ENUM_TYPE (FoundryCiRunState, foundry_ci_run_state,
                    G_DEFINE_ENUM_VALUE (FOUNDRY_CI_RUN_STATE_PENDING, "pending"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_CI_RUN_STATE_PREPARING, "preparing"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_CI_RUN_STATE_RUNNING, "running"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_CI_RUN_STATE_PASSED, "passed"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_CI_RUN_STATE_FAILED, "failed"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_CI_RUN_STATE_CANCELLED, "cancelled"))
