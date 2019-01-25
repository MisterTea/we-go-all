import * as React from 'react';
//import axios from 'axios';
import './App.css';
import { observer } from 'mobx-react';
import DevTools from 'mobx-react-devtools';
import logo from './logo.svg';
import AppState from './AppState';
import { Alert } from 'react-bootstrap';
import { FontAwesomeIcon } from '@fortawesome/react-fontawesome'
import { faDiscord, faGithub } from '@fortawesome/free-brands-svg-icons'

@observer
class App extends React.Component<{ appState: AppState }, { [username: string]: string, password: string }> {
  form: any;
  loginType: string | null;

  constructor(props: Readonly<{ appState: AppState }>) {
    super(props);
    this.state = {
      username: "",
      password: "",
    };
    this.form = null;
    this.loginType = null;

    this.handleChange = this.handleChange.bind(this);
    this.handleLoginSubmit = this.handleLoginSubmit.bind(this);
  }

  handleLoginSubmit(event: React.FormEvent<HTMLFormElement>) {
    console.log(event);
    if (this.loginType === "discord") {
      this.props.appState.loginDiscord();
    } else if (this.loginType === "github") {
      this.props.appState.loginGithub();
    } else {
      console.log(this.state.username);
      console.log(this.state.password);
      this.props.appState.login(this.state.username, this.state.password);
    }
    event.preventDefault();
  }

  handleChange(event: React.ChangeEvent<HTMLInputElement>) {
    if (event.target.name !== "username" && event.target.name !== "password") {
      throw new Error("invalid request");
    }
    let name: "username" | "password" = event.target.name;
    this.setState({ [name]: event.target.value });
  }

  public render() {
    var body;
    if (this.props.appState.userId === null) {
      body = (
        <form role="form" className="form" onSubmit={this.handleLoginSubmit} ref={el => this.form = el}>
          <div className="form-group">
            <label htmlFor="data">Username</label>
            <input type="text" name="username" value={this.state.username} onChange={this.handleChange}></input>
            <label htmlFor="data">Password</label>
            <input type="password" name="password" value={this.state.password} onChange={this.handleChange}></input>
          </div>
          <a className="btn btn-social btn-twitter" onClick={() => {
            this.loginType = "discord";
            this.form.dispatchEvent(new Event('submit'))
          }} >
            <span><FontAwesomeIcon icon={faDiscord} /></span>Sign in with Discord
          </a>
          <a className="btn btn-social btn-github" onClick={() => {
            this.loginType = "github";
            this.form.dispatchEvent(new Event('submit'))
          }} >
            <span><FontAwesomeIcon icon={faGithub} /></span>Sign in with Github
          </a>
        </form >
      );
    } else {
      body = (
        <p className="App-intro">
          To get started, eddddddit <code>src/App.tsx</code> and save to reload.
        </p>
      );
    }

    var alert = null;
    if (this.props.appState.errorMessages.length > 0) {
      alert = (
        <Alert bsStyle="warning" className="alert-fixed">
          {this.props.appState.errorMessages[0]}
        </Alert>
      );
    }
    return (
      <div className="App">
        <header className="App-header">
          <img src={logo} className="App-logo" alt="logo" />
          <h1 className="App-title">Welcome to React: {this.props.appState.timer}</h1>
        </header>
        {alert}
        {body}
        <DevTools />
      </div>
    );
  }
}

export default App;
