#!/usr/bin/env python3
"""main専用run203入口。既201の状態機械/CRC/実行条件を共通engineから使用。"""
import importlib.util
from pathlib import Path

spec=importlib.util.spec_from_file_location('gpu368_controller',Path(__file__).with_name('run-gpu368-fresh200.py'))
engine=importlib.util.module_from_spec(spec)
spec.loader.exec_module(engine)
Observation=engine.Observation
if __name__=='__main__':engine.main('run203',__file__)
