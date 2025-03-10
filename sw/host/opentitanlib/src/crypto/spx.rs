// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
use std::io::{Read, Write};

use anyhow::Result;

use pqcrypto_sphincsplus::sphincsshake256128ssimple as spx;
use pqcrypto_traits::sign::DetachedSignature;
use pqcrypto_traits::sign::PublicKey;
use pqcrypto_traits::sign::SecretKey;

use crate::util::bigint::fixed_size_bigint;
use crate::util::file::{FromReader, PemSerilizable, ToWriter};

// Signature and key sizes taken from Table 8 on page 57 of the SPHINCS+ Round 3 Specification:
// https://sphincs.org/data/sphincs+-round3-specification.pdf
const PUBLIC_KEY_BYTE_LEN: usize = 32;
const SECRET_KEY_BYTE_LEN: usize = 64;
const SIGNATURE_BIT_LEN: usize = 7856 * 8;
fixed_size_bigint!(Signature, at_most SIGNATURE_BIT_LEN);

/// Trait for implementing public key operations.
pub trait SpxPublicKeyPart {
    /// Returns the public key component.
    fn pk(&self) -> &spx::PublicKey;

    fn pk_as_bytes(&self) -> &[u8] {
        self.pk().as_bytes()
    }

    fn pk_len(&self) -> usize {
        self.pk_as_bytes().len()
    }

    /// Verify a message signature, returning Ok(()) if the signature matches.
    fn verify(&self, message: &[u8], sig: &SpxSignature) -> Result<()> {
        spx::verify_detached_signature(
            &spx::DetachedSignature::from_bytes(&sig.0.to_be_bytes())?,
            message,
            self.pk(),
        )?;
        Ok(())
    }
}

/// A SPHINCS+ keypair consisting of the public and secret keys.
#[derive(Clone)]
pub struct SpxKeypair {
    pk: spx::PublicKey,
    sk: spx::SecretKey,
}

impl SpxKeypair {
    /// Generates a new SPHINCS+ keypair.
    pub fn generate() -> Self {
        let (pk, sk) = spx::keypair();
        SpxKeypair { pk, sk }
    }

    /// Sign `message` using the secret key.
    pub fn sign(&self, message: &[u8]) -> SpxSignature {
        let sm = spx::detached_sign(message, &self.sk);
        SpxSignature(Signature::from_be_bytes(sm.as_bytes()).unwrap())
    }

    /// Consumes this keypair and returns the corrisponding public key.
    pub fn into_public_key(self) -> SpxPublicKey {
        SpxPublicKey(self.pk)
    }
}

impl SpxPublicKeyPart for SpxKeypair {
    fn pk(&self) -> &spx::PublicKey {
        &self.pk
    }
}

impl ToWriter for SpxKeypair {
    fn to_writer(&self, w: &mut impl Write) -> Result<()> {
        // Write out the keypair as a fixed length byte-string consisting of the public key
        // concatenated with the secret key.
        w.write_all(self.pk.as_bytes())?;
        w.write_all(self.sk.as_bytes())?;
        Ok(())
    }
}

impl FromReader for SpxKeypair {
    fn from_reader(mut r: impl Read) -> Result<Self> {
        // Read in the buffer as a fixed length byte-string consisting of the public key
        // concatenated with the secret key.
        let mut buf = [0u8; PUBLIC_KEY_BYTE_LEN + SECRET_KEY_BYTE_LEN];
        r.read_exact(&mut buf)?;
        Ok(SpxKeypair {
            pk: spx::PublicKey::from_bytes(&buf[..PUBLIC_KEY_BYTE_LEN])?,
            sk: spx::SecretKey::from_bytes(&buf[PUBLIC_KEY_BYTE_LEN..])?,
        })
    }
}

impl PemSerilizable for SpxKeypair {
    fn label() -> &'static str {
        "RAW SPHINCS+ PRIVATE KEY"
    }
}

/// Wrapper for a SPHINCS+ public key.
#[derive(Clone)]
pub struct SpxPublicKey(spx::PublicKey);

impl SpxPublicKeyPart for SpxPublicKey {
    fn pk(&self) -> &spx::PublicKey {
        &self.0
    }
}

impl ToWriter for SpxPublicKey {
    fn to_writer(&self, w: &mut impl Write) -> Result<()> {
        w.write_all(self.0.as_bytes())?;
        Ok(())
    }
}

impl FromReader for SpxPublicKey {
    fn from_reader(mut r: impl Read) -> Result<Self> {
        let mut buf = [0u8; PUBLIC_KEY_BYTE_LEN];
        r.read_exact(&mut buf)?;
        Ok(SpxPublicKey(spx::PublicKey::from_bytes(&buf)?))
    }
}

impl PemSerilizable for SpxPublicKey {
    fn label() -> &'static str {
        "RAW SPHINCS+ PUBLIC KEY"
    }
}

/// Wrapper for a SPHINCS+ signature.
#[derive(Clone)]
pub struct SpxSignature(Signature);

impl ToWriter for SpxSignature {
    fn to_writer(&self, w: &mut impl Write) -> Result<()> {
        w.write_all(&self.0.to_le_bytes())?;
        Ok(())
    }
}

impl FromReader for SpxSignature {
    fn from_reader(mut r: impl Read) -> Result<Self> {
        let mut buf = [0u8; SIGNATURE_BIT_LEN / 8];
        let len = r.read(&mut buf)?;
        Ok(SpxSignature(Signature::from_le_bytes(&buf[..len])?))
    }
}

impl ToString for SpxSignature {
    fn to_string(&self) -> String {
        self.0.to_string()
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_spx_sign() {
        let msg = b"Test message";

        let keypair = SpxKeypair::generate();
        let sig = keypair.sign(msg);
        assert!(keypair.verify(msg, &sig).is_ok());
    }
}
