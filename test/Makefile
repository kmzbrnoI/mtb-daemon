test:
	# Specify reasonable tests order
	pytest test_version.py test_mtbusb.py test_module.py

lint:
	-flake8 *.py
	-mypy --strict *.py

.PHONY: test lint
