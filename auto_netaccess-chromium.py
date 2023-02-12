#!/bin/python
"""
Created By:
    Omkar Tupe
Requirements can be installed via:
    pip install -U selenium
    # Tested with selenium==4.5.0
Set the USERNAME and PASSWORD at _line 30_ before proceeding
"""

from webdriver_manager.chrome import ChromeDriverManager
from selenium import webdriver
from selenium.webdriver.chrome.service import Service as ChromeService
from selenium.webdriver.common.by import By
import re
chrome_options = webdriver.ChromeOptions()
chrome_options.add_argument('--headless')
chrome_options.add_argument('--no-sandbox')
chrome_options.add_argument('--disable-dev-shm-usage')
# browser = webdriver.Chrome(options=chrome_options,
#     service=ChromeService(ChromeDriverManager().install()))
browser = webdriver.Chrome(options=chrome_options)
browser.implicitly_wait(5)
browser.get("https://netaccess.iitm.ac.in/account/login")

print(browser.current_url)
elem = browser.find_element(By.NAME,"userLogin").send_keys('USERNAME')
elem = browser.find_element(By.NAME,"userPassword").send_keys('PASSWORD' + Keys.RETURN)
time.sleep(10)
elem = browser.get('https://netaccess.iitm.ac.in/account/approve')
time.sleep(10)
elem = browser.find_element(By.ID, "radios-1").click()
time.sleep(1)
elem = browser.find_element(By.ID, "approveBtn").click()
time.sleep(2)
print("Closing the driver")
driver.quit()
# print(browser.current_url)
# elem = browser.find_element(By.NAME, "submit").click()

