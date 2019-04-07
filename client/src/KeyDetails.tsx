import * as React from 'react';
//import axios from 'axios';
import './App.css';
import { observer } from 'mobx-react';
import AppState from './AppState';

@observer
class KeyDetails extends React.Component<{ appState: AppState }, { [newKey: string]: string }> {
  form: any;
  appState: AppState;

  constructor(props: Readonly<{ appState: AppState }>) {
    super(props);
    this.appState = this.props.appState;
    this.state = {
    };
    this.form = null;

    this.handleSubmit = this.handleSubmit.bind(this);
    this.handleChange = this.handleChange.bind(this);
  }

  handleSubmit(event: React.FormEvent<HTMLFormElement>) {
    console.log(event);
    this.props.appState.setPublicKey(this.appState.publicKey);
    event.preventDefault();
  }

  handleChange(event: React.ChangeEvent<HTMLInputElement>) {
    if (event.target.name !== "publicKey") {
      throw new Error("invalid request");
    }
    let name: "publicKey" = event.target.name;
    this.setState({ [name]: event.target.value });
  }

  public render() {
    return (
      <div className="KeyDetails">
        <form role="form" className="form" onSubmit={this.handleSubmit} ref={el => this.form = el}>
          <div className="form-group">
            <label htmlFor="data">Public Key</label>
            <input type="text" name="publicKey" value={this.appState.publicKey} onChange={this.handleChange}></input>
            <a className="btn" onClick={() => {
              this.form.dispatchEvent(new Event('submit'))
            }} >Update Public Key</a>
          </div>
        </form>
      </div>
    );
  }
}

export default KeyDetails;