import * as React from 'react';
//import axios from 'axios';
import './App.css';
import { observer } from 'mobx-react';
import AppState from './AppState';
import { sha256 } from 'js-sha256';
import Form from 'react-bootstrap/Form';
import { Button } from 'react-bootstrap';


declare var require: (moduleId: string) => any;
var sodium = require('sodium-javascript');

@observer
class UserDetails extends React.Component<{ appState: AppState }, { [newKey: string]: string }> {
  form: any;
  appState: AppState;

  constructor(props: Readonly<{ appState: AppState }>) {
    super(props);
    this.appState = this.props.appState;
    this.state = {
      publicKey: this.props.appState.publicKey,
      privateKey: "",
      name: this.props.appState.name,
    };
    this.form = null;

    this.handleSubmit = this.handleSubmit.bind(this);
    this.privateKeyChange = this.privateKeyChange.bind(this);
  }

  handleSubmit(event: React.FormEvent<HTMLFormElement>) {
    console.log(event);
    this.props.appState.updateMe(this.state.publicKey, this.state.name);
    event.preventDefault();
  }

  privateKeyChange(event: React.ChangeEvent<HTMLInputElement>) {
    if (event.target.name === "privateKey") {
      if (event.target.value === "") {
        this.setState({
          privateKey: "",
          publicKey: "",
        });
      } else {
        console.log("HANDLING CHANGE");
        const fromHexString = (hexString: string) => {
          let match = hexString.match(/.{1,2}/g);
          if (match === null) {
            throw "Invalid hex string: " + hexString;
          }
          return new Uint8Array(match.map(byte => parseInt(byte, 16)));
        }
        let privateKey = fromHexString(sha256(event.target.value + "/" + this.props.appState.userId));
        let publicKey = new Uint8Array(sodium.crypto_scalarmult_BYTES);
        console.log(sodium);
        console.dir(sodium);
        console.log(privateKey.length);
        console.log(publicKey.length);
        console.log(privateKey);
        console.log(publicKey);
        sodium.crypto_scalarmult_base(publicKey, privateKey);
        console.log(privateKey);
        console.log(publicKey);
        this.setState({
          privateKey: event.target.value,
          publicKey: btoa(String.fromCharCode.apply(null, publicKey)),
        });
      }
      return true;
    } else {
      throw new Error("invalid request");
    }
  }

  public render() {
    return (
      <div>
        <Form onSubmit={(e: any) => {
          e.preventDefault();
          this.props.appState.updateMe(this.state.publicKey, this.state.name);
        }}>
          <Form.Group>
            <Form.Label>User ID (put this in your mame.ini)</Form.Label>
            <Form.Control as="textarea" rows="3" name="userId" value={this.props.appState.userId ? this.props.appState.userId : ""} readOnly={true}></Form.Control>
          </Form.Group>
          <Form.Group>
            <Form.Label>Secret (put this in your mame.ini, DO NOT SHARE)</Form.Label>
            <Form.Control type="text" name="privateKey" placeholder="Enter a new secret to update" value={this.state.privateKey} onChange={this.privateKeyChange.bind(this)} autoComplete="off"></Form.Control>
          </Form.Group>
          <Form.Group>
            <Form.Label>Public Key (you may need this for debugging, but otherwise ignore)</Form.Label>
            <Form.Control as="textarea" rows="3" name="publicKey" value={this.state.publicKey ? this.state.publicKey : ""} readOnly={true}></Form.Control>
          </Form.Group>
          <Form.Group>
            <Form.Label>Displayed Name</Form.Label>
            <Form.Control type="text" name="name" value={this.state.name} onChange={(event: any) => { this.setState({ name: event.target.value }) }} autoComplete="off"></Form.Control>
          </Form.Group>
          <Button variant="primary" type="submit">
            Save Changes
          </Button>
        </Form>
      </div >
    );
  }
}

export default UserDetails;