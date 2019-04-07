import { observable } from 'mobx';
import axios from 'axios';
import * as Cookies from 'js-cookie';
import ApolloClient, { gql } from 'apollo-boost';

class AppState {
  @observable timer: number = 0;
  @observable userId: string | null = null;
  @observable errorMessages: string[] = [];
  @observable publicKey: string = "";
  @observable name: string = "Player";
  errorClearTime: number | null = null;
  client = new ApolloClient();

  static readonly ALERT_SHOW_TIME: number = 5;

  constructor() {
    let outerThis = this;
    let user_id = Cookies.get('user_id');
    if (user_id) {
      this.client.query({
        query: gql`
        {
          me {
            id
            name
            publicKey
            endpoints
          }
        }`
      }).then(({ data }) => {
        console.log("GOT DATA:");
        console.log(data.me);
        outerThis.userId = data.me.id;
        outerThis.name = data.me.name;
        outerThis.publicKey = data.me.publicKey;
        console.log(outerThis.publicKey);
      })

      setInterval(() => {
      }, 5000);
    }
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
      });
  }

  loginGithub() {
    console.log("LOGIN GITHUB");
    window.location.href = "/login/github";
  }

  loginDiscord() {
    window.location.href = "/login/discord";
  }

  setPublicKey(publicKey: string) {
    axios.post(`/api/set_user_info`, { publicKey })
      .then(res => {
        this.publicKey = publicKey;
      }).catch((error: Error) => {
        // handle error
        console.log(error);
      });
  }
}

export default AppState;
