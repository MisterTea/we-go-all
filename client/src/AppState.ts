import { observable } from 'mobx';
import axios from 'axios';

class AppState {
  @observable timer: number = 0;
  @observable userId: string | null = null;
  @observable errorMessages: string[] = [];
  errorClearTime: number | null = null;

  static readonly ALERT_SHOW_TIME: number = 5;

  constructor() {
    setInterval(() => {
      this.timer += 1;
      if (this.errorClearTime !== null && this.timer >= this.errorClearTime) {
        this.errorMessages.shift();
        if (this.errorMessages.length > 0) {
          this.errorClearTime = this.timer + AppState.ALERT_SHOW_TIME;
        } else {
          this.errorClearTime = null;
        }
      }
    }, 1000);
  }

  addErrorMessage(m: string) {
    if (this.errorClearTime === null) {
      this.errorClearTime = this.timer + AppState.ALERT_SHOW_TIME;
    }
    this.errorMessages.push(m);
  }

  login(username: string, password: string) {
    axios.post(`/login`, { username, password })
      .then(res => {
        console.log("GOT LOGIN RESULT");
        console.log(res);
        console.log(res.data);
        let objData = JSON.parse(res.data);
        this.userId = objData.userId;
      }).catch((error: Error) => {
        // handle error
        console.log(error);
        if (error.message.search("status code 401") >= 0) {
          this.addErrorMessage("Wrong username/password.");
        } else {
          this.addErrorMessage("Invalid Login Attempt: " + error);
        }
      })
  }

  loginGithub() {
    window.location.href = "/login/github";
  }

  loginDiscord() {
    window.location.href = "/login/discord";
  }
}

export default AppState;
