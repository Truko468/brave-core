# Copyright (c) 2024 The Brave Authors. All rights reserved.
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at https://mozilla.org/MPL/2.0/.

import logging
import os
import glob
import shutil

from typing import List

import components.perf_config as perf_config
import components.perf_test_runner as perf_test_runner
import components.perf_test_utils as perf_test_utils
import components.path_util as path_util

from components.common_options import CommonOptions

_HOSTS_TO_REMOVE = [
    'brave-core-ext.s3.brave.com',  # components downloading
    'go-updater.brave.com',  # components update check
    'redirector.brave.com',
    'optimizationguide-pa.googleapis.com',  # optimizationguide chrome component
    'safebrowsingohttpgateway.googleapis.com',  # safebrowsing update
]


def _run_httparchive(args: List[str]) -> None:
  perf_test_utils.GetProcessOutput(
      ['go', 'run', os.path.join('src', 'httparchive.go'), *args],
      cwd=os.path.join(path_util.GetCatapultDir(), 'web_page_replay_go'),
      check=True)


def _get_wpr_pattern() -> str:
  return path_util.GetPageSetsDataPath('*.wprgo')


def _clean_wpr_files() -> None:
  for file in glob.glob(_get_wpr_pattern()):
    os.unlink(file)


def _merge_wpr_files() -> str:
  files = []
  for file in glob.glob(_get_wpr_pattern()):
    files.append(file)

  if len(files) == 0:
    raise RuntimeError('No wprgo files to merge')

  output_file = files[-1]

  args = ['merge']
  args.extend(files)
  args.append(output_file)

  _run_httparchive(args)

  # clean the source files:
  os.unlink(output_file + '.sha1')
  for file in files[:-1]:
    os.unlink(file)
    os.unlink(file + '.sha1')

  return output_file


def _post_process_wpr(file: str) -> None:
  for host in _HOSTS_TO_REMOVE:
    _run_httparchive(['trim', '--host', host, file, file])

  _run_httparchive(['ls', file])


def RecordWpr(config: perf_config.PerfConfig, options: CommonOptions) -> bool:
  if len(config.runners) != 2:
    raise RuntimeError('Set two runners to record wpr: Brave and Chromium')
  options.do_report = False
  runable_configurations = perf_test_runner.PrepareBinariesAndDirectories(
      config.runners, config.benchmarks, options)

  _clean_wpr_files()

  for c in runable_configurations:
    c.Install()
    c.RebaseProfile()

    for benchmark in c.benchmarks:
      args = [path_util.GetVpython3Path()]
      args.append(os.path.join(path_util.GetChromiumPerfDir(), 'record_wpr'))
      args.append(benchmark.name)
      args.extend(c.binary.get_run_benchmark_args())

      if len(benchmark.stories) > 0:
        story_filter = '|'.join(benchmark.stories)
        args.append(f'--story-filter={story_filter}')

      perf_test_utils.GetProcessOutput(args,
                                       cwd=path_util.GetChromiumPerfDir(),
                                       timeout=360,
                                       check=True)

  output_file = _merge_wpr_files()
  _post_process_wpr(output_file)

  # Copy the final .wprgo to the artifacts directory.
  artifacts_dir = os.path.join(options.working_directory, 'artifacts')
  os.makedirs(artifacts_dir, exist_ok=True)
  shutil.copy(output_file, artifacts_dir)

  logging.info('Output file to upload: %s', output_file)

  return True
