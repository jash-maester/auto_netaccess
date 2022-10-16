#!/bin/python
"""
Created By:
    Jashaswimalya Acharjee

Requirements can be installed via:
    sudo apt install firefox-geckodriver -y
    pip install -U selenium
    # Tested with selenium==4.5.0

Set the USERNAME and PASSWORD at _line 38_ before proceeding
"""

from selenium import webdriver
from selenium.webdriver.firefox.firefox_profile import FirefoxProfile
from selenium.webdriver.firefox.options import Options
from selenium.webdriver.common.by import By
from selenium.webdriver.common.keys import Keys
from selenium.webdriver.firefox.service import Service
import time


profile = webdriver.FirefoxProfile()
profile.accept_untrusted_certs = True

options = Options()
options.headless = True
service = Service(log_path='/dev/null')

driver = webdriver.Firefox(firefox_profile=profile, options=options, service=service)

driver.get('https://netaccess.iitm.ac.in/')
time.sleep(10)

print(driver.title)

# Start injecting your credentials
elem = driver.find_element(By.NAME, 'userLogin').send_keys('USERNAME')
elem = driver.find_element(By.NAME, 'userPassword').send_keys('PASSWORD' + Keys.RETURN)

time.sleep(10)

elem = driver.get('https://netaccess.iitm.ac.in/account/approve')
time.sleep(10)
elem = driver.find_element(By.ID, "radios-1").click()
time.sleep(1)
elem = driver.find_element(By.ID, "approveBtn").click()
time.sleep(2)

print("Closing the driver")
driver.quit()
