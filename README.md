# Automatic Netaccess Authentication
To get internet access for Students and Employees at IITM, one needs to login to the Netaccess (i.e. IITM's Internet Access) Portal, which apparently lasts for 24hrs and then again you have to login else the credentials will get expired and thus no internet access for you at all.
So, out of frustration of repeatedly logging in everyday doing the same task like a moron, I created this script with Selenium Webdriver automation framework, with Firefox Geckodriver backbone (Because firefox is usually installed by default on Linux Systems).

# Prerequisites
Requirements can be installed via:
	```bash
    sudo apt install firefox-geckodriver -y
    pip install -U selenium
    # Tested with selenium==4.5.0
	```

# NOTE
Change the `USERNAME` and `PASSWORD` in the `.py` file and `execute` it. Read the script before executing, it's fairly straight-forward.

Later setup a cron job or a systemd-timer to automatically run it after a certain interval of time.
Not gonna go in details on how to set timers here, as it is operating system independent.
