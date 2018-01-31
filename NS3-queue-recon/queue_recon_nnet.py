#!/usr/bin/env python
import numpy as np
import sys
import argparse
import gc
import os
import matplotlib
matplotlib.use('Agg')
from pylab import *
from sklearn import svm
from sklearn import linear_model
import pickle
import tensorflow as tf

#Read data
delay_vector = np.loadtxt('delay_vector.txt')
queue_vector = np.loadtxt('queue_vector.txt')
delay_dim = len(delay_vector[0,:])
queue_dim = len(queue_vector[0,:])
print '# of samples =', len(delay_vector[:,0])
print 'delay_dim =', delay_dim
print 'queue_dim =', queue_dim

#Normalization, and split training/testing set
delay_mean = np.mean(delay_vector)
delay_std = np.std(delay_vector)
queue_mean = np.mean(queue_vector)
queue_std = np.std(queue_vector)
print delay_mean, delay_std, queue_mean, queue_std

len_train_set = int(len(delay_vector[:,0]) * 0.8)
print '# of training samples =', len_train_set
print '# of testing samples =', len(delay_vector[:,0])-len_train_set

delay_train = (delay_vector[:len_train_set,:] - delay_mean)/delay_std
queue_train = (queue_vector[:len_train_set,:] - queue_mean)/queue_std
delay_test = (delay_vector[len_train_set:,:] - delay_mean)/delay_std
queue_test = (queue_vector[len_train_set:,:] - queue_mean)/queue_std


def generate_batch(data_delay, data_queue, batch_size = 100):
  indices = np.random.choice(len(data_delay[:,0]), batch_size, replace=False)
  batch_xs = []
  batch_ys = []
  for i in indices:
    batch_xs.append( data_delay[i,:] )
    batch_ys.append( data_queue[i,:] )
  return np.array(batch_xs), np.array(batch_ys)


class model:
  def __init__(self, num_layer = 1, num_neuron = 96):
    self.num_layer = num_layer
    self.num_neuron = num_neuron
    self.proj_dim = queue_dim

  def layer(self, x, num_neuron):
    input_shape = x.get_shape()
    W = tf.get_variable("weights", 
      [input_shape[-1], num_neuron], dtype=tf.float64, initializer = tf.truncated_normal_initializer(stddev = 0.1))
    b = tf.get_variable("bias", 
      [1, num_neuron], dtype=tf.float64, initializer = tf.truncated_normal_initializer(stddev = 0.1))
    y = tf.matmul(x, W) + b
    return tf.nn.relu(y)
  
  def construct_graph(self):
    self.x = tf.placeholder(tf.float64, [None, delay_dim])
    h = self.x

    for layer_idx in range(self.num_layer):
      with tf.variable_scope("layer{}".format(layer_idx)):
        h1 = self.layer(x=h, num_neuron=self.num_neuron)
        h = h1
    with tf.variable_scope("proj_layer"):
      hidden_shape = h.get_shape()
      W = tf.get_variable("W", [hidden_shape[1], self.proj_dim], dtype=tf.float64, initializer=tf.truncated_normal_initializer(stddev=0.1))
      b = tf.get_variable("b", [self.proj_dim], dtype=tf.float64, initializer=tf.truncated_normal_initializer(stddev=0.1))
      y = tf.matmul(h, W) + b

    self.prediction = y
    self.y_ = tf.placeholder(tf.float64, [None, self.proj_dim])
    self.loss = tf.reduce_mean(tf.square(y - self.y_))
    self.train_step = tf.train.AdamOptimizer(1e-4).minimize(self.loss)
    self.saver = tf.train.Saver()
    
  def train(self, sess, delay_train, queue_train, delay_test, queue_test):
    train_loss_history = []
    test_loss_history = []
    tf.global_variables_initializer().run()
    batch_size = 100
    for i in range(100000):
      test_loss_history.append( self.test(sess, delay_test, queue_test) )
      batch_xs, batch_ys = generate_batch(delay_train, queue_train, batch_size)
      loss, _ = sess.run([self.loss, self.train_step], feed_dict={self.x: batch_xs, self.y_: batch_ys})
      print 'iteration =', i, ' train loss =', loss
      train_loss_history.append(loss)
      
    save_path = self.saver.save(sess, "./model.ckpt")

    return train_loss_history, test_loss_history

  def test(self, sess, data_delay, data_queue, num_tests = 100):
    #self.saver.restore(sess, "./model.ckpt")
    batch_xs, batch_ys = generate_batch(data_delay, data_queue, num_tests)
    loss, pred = sess.run([self.loss, self.prediction], feed_dict = {self.x: batch_xs, self.y_: batch_ys})
    print 'test loss: {}'.format(loss)
    #print 'pred[0].shape[0]={}'.format(pred[0].shape[0])
    return loss

  def test_final(self, sess, data_delay, data_queue, plotfig=False):
    #self.saver.restore(sess, "./model.ckpt")
    loss, pred = sess.run([self.loss, self.prediction], feed_dict = {self.x: data_delay, self.y_: data_queue})
    print pred.shape
    print data_queue.shape

    pred_original = pred * queue_std + queue_mean
    data_queue_original = data_queue * queue_std + queue_mean

    if plotfig==True:
      for queueIndex in range(pred.shape[1]):
        figure()
        fill_between(range(200), pred_original[-200:,queueIndex], 0, color='red', alpha=0.8, label='by NN')
        plot(range(200), data_queue_original[-200:,queueIndex], color='blue', linewidth=2, label='NS3')
        legend()
        title('Queue {}'.format(queueIndex) )
        ylim([ 0, 1000000 ])
        xlabel('Time (x10 msec)')
        ylabel('Queue length (Bytes)')
        savefig('NNfig/fig'+str(queueIndex)+'.png')

    RMSE = math.sqrt( np.sum(np.square(pred_original - data_queue_original)) / data_queue_original.size )
    RMS = math.sqrt( np.sum(np.square(data_queue_original)) / data_queue_original.size )
    return RMSE, RMS

my_model = model(num_layer = 1, num_neuron = 96)
my_model.construct_graph()
sess = tf.InteractiveSession()

train_loss_history, test_loss_history = my_model.train(sess, delay_train, queue_train, delay_test, queue_test)
RMSE, RMS = my_model.test_final(sess, delay_test, queue_test, True)

print 'Queue recon RMSE =', RMSE, 'Bytes'
print 'Queue recon relative error =', RMSE/RMS

plt.figure()
plt.plot([i for i in range(len(train_loss_history))], train_loss_history, label='train loss')
plt.plot([i for i in range(len(test_loss_history))], test_loss_history, label='test loss')
plt.legend()
plt.grid()
plt.savefig('loss.pdf')

