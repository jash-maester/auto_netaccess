import requests
#import logging
from urllib.parse import urljoin

# Set up logging
#logging.basicConfig(
#    filename='netaccess_automation.log',
#    level=logging.INFO,
#    format='%(asctime)s - %(levelname)s - %(message)s'
#)

class NetAccessAuth:
    def __init__(self, base_url="https://netaccess.iitm.ac.in"):
        self.base_url = base_url
        self.session = requests.Session()
        # Set common headers to mimic a browser
        self.session.headers.update({
            'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36',
            'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8',
            'Accept-Language': 'en-US,en;q=0.5',
            'Accept-Encoding': 'gzip, deflate',
            'Connection': 'keep-alive',
            'Upgrade-Insecure-Requests': '1',
        })
    
    def login(self, username, password):
        """
        Perform login to netaccess
        """
        try:
            # First, get the login page to establish session
            login_url = urljoin(self.base_url, '/')
            response = self.session.get(login_url)
            
            if response.status_code != 200:
                #logging.error(f"Failed to access login page. Status code: {response.status_code}")
                return False
            
            #logging.info("Successfully accessed login page")
            
            # Prepare login data
            login_data = {
                'userLogin': username,
                'userPassword': password,
                'submit': 'Log in'
            }
            
            # Submit login form
            login_post_url = urljoin(self.base_url, '/account/login')
            response = self.session.post(login_post_url, data=login_data)
            
            if response.status_code != 200:
                #logging.error(f"Login failed. Status code: {response.status_code}")
                return False
            
            # Check if login was successful by looking for indicators in the response
            if 'logout' in response.text.lower() or 'authorized machines' in response.text.lower():
                #logging.info("Login successful")
                return True
            else:
                #logging.error("Login failed - invalid credentials or other error")
                return False
                
        except Exception as e:
            #logging.error(f"Error during login: {e}")
            return False
    
    def approve_machine(self, duration=2):
        """
        Approve the current machine for internet access
        duration: 1 for 1 hour, 2 for 1 day
        """
        try:
            # First, get the approve page
            approve_url = urljoin(self.base_url, '/account/approve')
            response = self.session.get(approve_url)
            
            if response.status_code != 200:
                #logging.error(f"Failed to access approve page. Status code: {response.status_code}")
                return False
            
            #logging.info("Successfully accessed approve page")
            
            # Prepare approval data
            approve_data = {
                'duration': str(duration),
                'approveBtn': 'Authorize'
            }
            
            # Submit approval form
            response = self.session.post(approve_url, data=approve_data)
            
            if response.status_code != 200:
                #logging.error(f"Machine approval failed. Status code: {response.status_code}")
                return False
            
            # Check for success indicators
            if 'success' in response.text.lower() or 'authorized' in response.text.lower():
                #logging.info("Machine approval successful")
                return True
            else:
                #logging.info("Machine approval request submitted (check response for confirmation)")
                return True
                
        except Exception as e:
            #logging.error(f"Error during machine approval: {e}")
            print(f"Error During Machine Approval: {e}")
            return False
    
    def get_authorized_machines(self):
        """
        Get list of authorized machines and usage info
        """
        try:
            # Access the main page after login to see authorized machines
            response = self.session.get(self.base_url)
            
            if response.status_code == 200:
                #logging.info("Successfully retrieved authorized machines info")
                return response.text
            else:
                #logging.error(f"Failed to get authorized machines. Status code: {response.status_code}")
                return None
                
        except Exception as e:
            #logging.error(f"Error getting authorized machines: {e}")
            return None

def main():
    """
    Main function to perform the complete automation
    """
    # Your credentials
    USERNAME = 'USERNAME'
    PASSWORD = 'PASSWORD'  # Note: Consider using environment variables for security
    
    try:
        # Initialize the authentication client
        auth = NetAccessAuth()
        
        # Perform login
        #logging.info("Starting netaccess automation")
        if not auth.login(USERNAME, PASSWORD):
            print("Login failed")
            #logging.error("Login failed")
            return False
        
        print("Login successful")
        
        # Approve machine for 1 day (duration=2)
        if not auth.approve_machine(duration=2):
            print("Machine approval failed")
            #logging.error("Machine approval failed")
            return False
        
        print("Machine approval successful")
        
        # Optional: Get authorized machines info
        machines_info = auth.get_authorized_machines()
        #if machines_info:
        #    logging.info("Retrieved authorized machines information")
        
        print("Automation completed successfully")
        #logging.info("Automation completed successfully")
        return True
        
    except Exception as e:
        print(f"Error occurred: {e}")
        #logging.error(f"Error occurred: {e}")
        return False

if __name__ == "__main__":
    success = main()
    if success:
        print("NetAccess automation completed successfully")
    else:
        print("NetAccess automation failed - check logs for details")
