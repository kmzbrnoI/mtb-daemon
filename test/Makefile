test:
	# Specify reasonable tests order
	pytest test_version.py test_mtbusb.py test_module.py test_uni.py test_events.py test_diag.py

test_manual:
	pytest test_manual.py

lint:
	-flake8 *.py
	-mypy --strict *.py

.PHONY: test lint
